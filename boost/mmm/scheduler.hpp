//          Copyright Kohei Takahashi 2011 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_SCHEDULER_HPP
#define BOOST_MMM_SCHEDULER_HPP

#include <cstddef>
#include <utility>
#include <memory>

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#include <boost/assert.hpp>
#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
#include <boost/mmm/detail/unused.hpp>
#endif
#include <boost/ref.hpp>
#include <boost/noncopyable.hpp>
#include <boost/mmm/detail/move.hpp>

#include <stdexcept>
#include <boost/throw_exception.hpp>

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/enum_trailing_params.hpp>
#include <boost/preprocessor/repetition/enum_trailing_binary_params.hpp>
#endif

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/mmm/detail/thread.hpp>
#include <boost/mmm/detail/context.hpp>
#include <boost/context/stack_utils.hpp>
#include <boost/fusion/include/at.hpp>

#include <boost/system/error_code.hpp>

#include <boost/atomic.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/mmm/detail/locks.hpp>

#include <boost/checked_delete.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include <boost/container/allocator/allocator_traits.hpp>
#endif
#include <functional>
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID) \
 && defined(BOOST_UNORDERED_USE_MOVE)
#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>
#else
#include <boost/container/map.hpp>
#endif

#include <boost/mmm/detail/current_context.hpp>
#include <boost/mmm/detail/context_guard.hpp>
#include <boost/mmm/strategy_traits.hpp>
#include <boost/mmm/scheduler_traits.hpp>

namespace boost { namespace mmm {

namespace detail {

template <typename StrategyTraits, typename Allocator>
struct scheduler_data : private noncopyable
{
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<Allocator> allocator_traits;
#endif

    template <typename Key, typename Elem>
    struct map_type
    {
        typedef std::pair<const Key, Elem> value_type;
        typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
          typename allocator_traits::template rebind_alloc<value_type>
#else
          typename Allocator::template rebind<value_type>::other
#endif
        alloc_type;

        typedef
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
          unordered_map<Key, Elem, hash<Key>, std::equal_to<Key>, alloc_type>
#else
          container::map<Key, Elem, std::less<Key>, alloc_type>
#endif
        type;
    }; // template struct map_type

    typedef typename map_type<thread::id, thread>::type kernels_type;
    typedef typename StrategyTraits::pool_type users_type;

    atomic<int>        status;
    unsigned           runnings;
    mutex              mtx;
    condition_variable cond;
    kernels_type       kernels;
    users_type         users;

    scheduler_data()
      : status(0), runnings(0) {}
}; // struct scheduler_data

} // namespace boost::mmm::detail

template <typename Strategy, typename Allocator = std::allocator<void> >
class scheduler
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(scheduler)

public:
    typedef Strategy strategy_type;
    typedef Allocator allocator_type;
    typedef std::size_t size_type;

private:
    typedef scheduler this_type;

    friend class detail::context_guard<this_type>;
    friend struct scheduler_traits<this_type>;

    enum scheduler_status
    {
        _st_terminate = 1 << 0,
        _st_join      = 1 << 1,

        _st_none = 0
    }; // enum scheduler_status

    typedef mmm::detail::context_guard<this_type> context_guard;

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<allocator_type> allocator_traits;
#endif
    typedef mmm::scheduler_traits<this_type> scheduler_traits;
    typedef
      mmm::strategy_traits<strategy_type, detail::context_tuple, allocator_type>
    strategy_traits;

    typedef detail::scheduler_data<strategy_traits, allocator_type> scheduler_data;
    typedef typename scheduler_data::kernels_type kernels_type;
    typedef typename scheduler_data::users_type users_type;

public:
    typedef typename strategy_traits::context_type context_type;

private:
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    void
    _m_resume_context(unique_lock<mutex> &guard, context_type &ctx)
    {
        using namespace detail;
        unique_unlock<mutex> unguard(guard);

        io_callback_base *callback = fusion::at_c<1>(ctx);
        if (callback && !callback->done())
        {
            system::error_code err_code;
            if (callback->check_events(err_code))
            {
                callback->operator()();
            }
        }
        else
        {
            current_context::set_current_ctx(&ctx);
            fusion::at_c<0>(ctx).resume();
            current_context::set_current_ctx(0);
        }
    }

    void
    _m_exec(scheduler_data &data)
    {
        while (!(data.status & _st_terminate))
        {
            // Lock until to be able to get least one context.
            unique_lock<mutex> guard(data.mtx);
            // Check and breaking loop when destructing scheduler.
            while (!(data.status & _st_terminate) && !data.users.size())
            {
                // TODO: Check interrupts.
                data.cond.wait(guard);
            }
            if (data.status & _st_terminate) { break; }

            context_guard ctx_guard(scheduler_traits(*this), strategy_traits());

            ++data.runnings;
            _m_resume_context(guard, ctx_guard.context());
            --data.runnings;

            // Notify all even if context is finished to wakeup caller of join_all.
            if (data.status & _st_join)
            {
                // NOTICE: Because of using notify_all instead of notify_one,
                // notify_one might wake up not caller of join_all.
                data.cond.notify_all();
            }
            // Notify one when context is not finished.
            else if (ctx_guard)
            {
                data.cond.notify_one();
            }
        }
    }

    // To run context should call start(). However, cannot get informations
    // about the context was started.
    struct context_starter
    {
        reference_wrapper<context_type> _m_ctx;

        explicit
        context_starter(context_type &ctx)
          : _m_ctx(ctx) {}

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#define BOOST_MMM_context_starter_op_call(unused_z_, n_, unused_data_)  \
        template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)> \
        void                                                            \
        operator()(Fn &fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, &arg)) const \
        {                                                               \
            fusion::at_c<0>(unwrap_ref(_m_ctx)).suspend();              \
            fn(BOOST_PP_ENUM_PARAMS(n_, arg));                          \
        }                                                               \
// BOOST_MMM_context_starter_op_call
        BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTEXT_ARITY), BOOST_MMM_context_starter_op_call, ~)
#undef BOOST_MMM_context_starter_op_call
#else
        template <typename Fn, typename... Args>
        void
        operator()(Fn &fn, Args &... args) const
        {
            fusion::at_c<0>(unwrap_ref(_m_ctx)).suspend();
            fn(args...);
        }
#endif
    }; // template struct context_starter

    static void
    start_context(context_type &ctx)
    {
        using namespace detail;

        current_context::set_current_ctx(&ctx);
        fusion::at_c<0>(ctx).start();
        current_context::set_current_ctx(0);
    }
#endif

public:
    /**
     * <b>Effects</b>: Move internal scheduler datas from the other. And the
     * other becomes <i>not-in-scheduling</i>.
     *
     * <b>Throws</b>: Nothing.
     */
    scheduler(BOOST_RV_REF(scheduler) other)
      : _m_data(move(other._m_data)) {}

    /**
     * <b>Effects</b>: Construct with specified count <i>kernel-threads</i>.
     *
     * <b>Throws</b>: std::invalid_argument (wrapped by Boost.Exception):
     * if default_count <= 0 .
     *
     * <b>Postcondition</b>: *this is not <i>not-in-scheduling</i>.
     */
    explicit
    scheduler(const int default_count)
    {
        if (!(0 < default_count))
        {
            using std::invalid_argument;
            BOOST_THROW_EXCEPTION(invalid_argument("default_count should be > 0"));
        }

        // XXX: Should use specified allocator.
        // Defer initializing.
        _m_data.reset(new scheduler_data);
        BOOST_ASSERT(_m_data);

        for (int cnt = 0; cnt < default_count; ++cnt)
        {
            thread th(&scheduler::_m_exec, boost::ref(*this), boost::ref(*_m_data));

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            std::pair<typename kernels_type::iterator, bool> r =
#endif
            // Call Boost.Move's boost::move via ADL
            _m_data->kernels.emplace(th.get_id(), move(th));
#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            BOOST_ASSERT(r.second);
            BOOST_MMM_DETAIL_UNUSED(r);
#endif
        }
#if defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
        BOOST_ASSERT(_m_data->kernels.size() == default_count);
#endif
    }

    /**
     * <b>Effects</b>: Join all <i>kernel-threads</i>. Call std::terminate immediately
     * iff joinable. Users should join all <i>user-threads</i> before destruct.
     *
     * <b>Throws</b>: Nothing.
     */
    ~scheduler()
    {
        if (!_m_data) { return; }

        if (joinable()) { std::terminate(); }

        {
            unique_lock<mutex> guard(_m_data->mtx);
            _m_data->status |= _st_terminate;
            _m_data->cond.notify_all();
        }

        typedef typename kernels_type::iterator iterator;
        typedef typename kernels_type::const_iterator const_iterator;

        const_iterator end = _m_data->kernels.end();
        for (iterator itr = _m_data->kernels.begin(); itr != end; ++itr)
        {
            itr->second.join();
        }
    }

    // To prevent unnecessary coping, explicit instantiation with lvalue reference.
#if !defined(BOOST_MMM_DOXYGEN_INVOKED) && defined(BOOST_NO_VARIADIC_TEMPLATES)
#define BOOST_MMM_scheduler_add_thread(unused_z_, n_, unused_data_)         \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    typename disable_if<is_same<size_type, Fn> >::type                      \
    add_thread(Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg))    \
    {                                                                       \
        BOOST_ASSERT(_m_data);                                              \
        add_thread<Fn & BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, & BOOST_PP_INTERCEPT)>( \
          contexts::default_stacksize()                                     \
        , fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg));                       \
    }                                                                       \
                                                                            \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    void                                                                    \
    add_thread(size_type size, Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg)) \
    {                                                                       \
        BOOST_ASSERT(_m_data);                                              \
        using contexts::no_stack_unwind;                                    \
        using contexts::return_to_caller;                                   \
                                                                            \
        context_type ctx;                                                   \
        contexts::context(                                                  \
          context_starter(ctx), fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg)   \
        , size, no_stack_unwind, return_to_caller).swap(fusion::at_c<0>(ctx)); \
        start_context(ctx);                                                 \
                                                                            \
        unique_lock<mutex> guard(_m_data->mtx);                             \
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));     \
        _m_data->cond.notify_one();                                         \
    }                                                                       \
// BOOST_MMM_scheduler_add_thread
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTEXT_ARITY), BOOST_MMM_scheduler_add_thread, ~)
#undef BOOST_MMM_scheduler_add_thread
#else
    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Effects</b>: Construct context and join to scheduling with default
     * stack size.
     *
     * <b>Requires</b>: All of functor and arguments are <b>CopyConstructible</b>.
     */
    template <typename Fn, typename... Args>
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    typename disable_if<is_same<size_type, Fn> >::type
#else
    void
#endif
    add_thread(Fn fn, Args... args)
    {
        BOOST_ASSERT(_m_data);
        using contexts::default_stacksize;
        add_thread<Fn &, Args &...>(default_stacksize(), fn, args...);
    }

    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Effects</b>: Construct context and join to scheduling with specified
     * stack size.
     *
     * <b>Requires</b>: All of functor and arguments are <b>CopyConstructible</b>.
     */
    template <typename Fn, typename... Args>
    void
    add_thread(size_type size, Fn fn, Args... args)
    {
        BOOST_ASSERT(_m_data);
        using contexts::no_stack_unwind;
        using contexts::return_to_caller;

        context_type ctx;
        contexts::context(
          context_starter(ctx), fn, args...
        , size, no_stack_unwind, return_to_caller).swap(fusion::at_c<0>(ctx));
        start_context(ctx);

        unique_lock<mutex> guard(_m_data->mtx);
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));
        _m_data->cond.notify_one();
    }
#endif

private:
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    bool
    joinable_nolock() const BOOST_NOEXCEPT
    {
        return _m_data->users.size() != 0 || _m_data->runnings != 0;
    }
#endif
public:
    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Effects</b>: Join all <i>user-threads</i>.
     *
     * <b>Throws</b>: Nothing.
     *
     * <b>Postcondition</b>: !joinable()
     */
    void
    join_all() BOOST_NOEXCEPT
    {
        BOOST_ASSERT(_m_data);
        unique_lock<mutex> guard(_m_data->mtx);

        while (joinable_nolock())
        {
            _m_data->status |= _st_join;
            _m_data->cond.wait(guard);
        }
        _m_data->status &= ~_st_join;
    }

    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Returns</b>: true iff any contexts are still not completed. Otherwise
     * false.
     *
     * <b>Throws</b>: Nothing.
     */
    bool
    joinable() const BOOST_NOEXCEPT
    {
        BOOST_ASSERT(_m_data);
        unique_lock<mutex> guard(_m_data->mtx);
        return joinable_nolock();
    }

    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Returns</b>: Number of <i>kernel-threads</i>.
     *
     * <b>Throws</b>: Nothing.
     */
    size_type
    kernel_size() const BOOST_NOEXCEPT
    {
        BOOST_ASSERT(_m_data);
        unique_lock<mutex> guard(_m_data->mtx);
        return _m_data->kernels.size();
    }

    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Returns</b>: Number of <i>user-threads</i>.
     *
     * <b>Throws</b>: Nothing.
     */
    size_type
    user_size() const BOOST_NOEXCEPT
    {
        BOOST_ASSERT(_m_data);
        unique_lock<mutex> guard(_m_data->mtx);
        return _m_data->users.size();
    }

private:
    // XXX: Should use specified allocator.
    typedef
      interprocess::unique_ptr<scheduler_data, checked_deleter<scheduler_data> >
    scheduler_data_ptr;

    scheduler_data_ptr _m_data;
}; // template class scheduler

} } // namespace boost::mmm

#endif

