//          Copyright Kohei Takahashi 2011 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_SCHEDULER_HPP
#define BOOST_MMM_SCHEDULER_HPP

#include <cstddef>
#include <utility>
#include <memory>
#include <functional>
#include <exception>
#include <stdexcept>

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
#include <boost/assert.hpp>
#include <boost/mmm/detail/unused.hpp>
#endif
#include <boost/ref.hpp>
#include <boost/noncopyable.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/throw_exception.hpp>

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/enum_trailing_params.hpp>
#include <boost/preprocessor/repetition/enum_trailing_binary_params.hpp>
#endif

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_same.hpp>

#include <boost/thread/thread.hpp>
#include <boost/mmm/detail/movable_thread.hpp>
#include <boost/context/context.hpp>
#include <boost/context/stack_utils.hpp>

#include <boost/atomic.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/mmm/detail/locks.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include <boost/container/allocator/allocator_traits.hpp>
#endif
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
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

template <typename Strategy, typename Allocator = std::allocator<void> >
class scheduler : private noncopyable
{
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
      mmm::strategy_traits<strategy_type, contexts::context, allocator_type>
    strategy_traits;

public:
    typedef typename strategy_traits::context_type context_type;

private:
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    void
    _m_exec()
    {
        strategy_traits traits;

        while (!(_m_status & _st_terminate))
        {
            // Lock until to be able to get least one context.
            unique_lock<mutex> guard(_m_mtx);
            // Check and breaking loop when destructing scheduler.
            while (!(_m_status & _st_terminate) && !_m_users.size())
            {
                // TODO: Check interrupts.
                _m_cond.wait(guard);
            }
            if (_m_status & _st_terminate) { break; }

            context_guard ctx_guard(scheduler_traits(*this), traits);

            ++_m_runnings;
            {
                using namespace detail;
                unique_unlock<mutex> unguard(guard);
                context_type &ctx = ctx_guard.context();

                current_context::set_current_ctx(&ctx);
                ctx.resume();
                current_context::set_current_ctx(0);
            }
            --_m_runnings;

            // Notify all even if context is finished to wakeup caller of join_all.
            if (_m_status & _st_join)
            {
                // NOTICE: Because of using notify_all instead of notify_one,
                // notify_one might wake up not caller of join_all.
                _m_cond.notify_all();
            }
            // Notify one when context is not finished.
            else if (ctx_guard)
            {
                _m_cond.notify_one();
            }
        }
    }

    // To run context should call start(). However, cannot get informations
    // about the context was started.
    template <typename F>
    struct context_starter
    {
        F _m_functor;
        reference_wrapper<context_type> _m_ctx;

        explicit
        context_starter(const F &functor, context_type &ctx)
          : _m_functor(functor), _m_ctx(ctx) {}

        void
        operator()() const
        {
            _m_ctx.get().suspend();
            _m_functor();
        }
    }; // template struct context_starter

    template <typename F>
    static context_starter<F>
    make_context_starter(const F &f, context_type &ctx)
    {
        return context_starter<F>(f, ctx);
    }

    void *
    start_context(context_type &ctx)
    {
        using namespace detail;

        current_context::set_current_ctx(&ctx);
        void *vp = ctx.start();
        current_context::set_current_ctx(0);
        return vp;
    }
#endif

public:
    /**
     * <b>Effects</b>: Construct with specified count <i>kernel threads</i>.
     *
     * <b>Throws</b>: std::invalid_argument (wrapped by Boost.Exception):
     * if default_count <= 0 .
     */
    explicit
    scheduler(const int default_count)
      : _m_status(_st_none), _m_runnings(0)
    {
        if (!(0 < default_count))
        {
            using std::invalid_argument;
            BOOST_THROW_EXCEPTION(invalid_argument("default_count should be > 0"));
        }

        for (int cnt = 0; cnt < default_count; ++cnt)
        {
            thread th(&scheduler::_m_exec, boost::ref(*this));

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            std::pair<typename kernels_type::iterator, bool> r =
#endif
            // Call Boost.Move's boost::move via ADL
            _m_kernels.emplace(th.get_id(), move(th));
#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            BOOST_ASSERT(r.second);
            BOOST_MMM_DETAIL_UNUSED(r);
#endif
        }
#if defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
        BOOST_ASSERT(_m_kernels.size() == default_count);
#endif
    }

    /**
     * <b>Precondition</b>: !joinable()
     *
     * <b>Effects</b>: Join all <i>kernel threads</i>. Call std::terminate immediately
     * iff joinable. Users should join all <i>user threads</i> before destruct.
     *
     * <b>Throws</b>: Nothing.
     */
    ~scheduler()
    {
        if (joinable()) { std::terminate(); }

        {
            unique_lock<mutex> guard(_m_mtx);
            _m_status |= _st_terminate;
            _m_cond.notify_all();
        }

        typedef typename kernels_type::iterator iterator;
        typedef typename kernels_type::const_iterator const_iterator;

        const_iterator end = _m_kernels.end();
        for (iterator itr = _m_kernels.begin(); itr != end; ++itr)
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
        add_thread<Fn & BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, & BOOST_PP_INTERCEPT)>( \
          contexts::default_stacksize()                                     \
        , fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg));                       \
    }                                                                       \
                                                                            \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    void                                                                    \
    add_thread(size_type size, Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg)) \
    {                                                                       \
        using contexts::no_stack_unwind;                                    \
        using contexts::return_to_caller;                                   \
                                                                            \
        context_type ctx;                                                   \
        context_type(                                                       \
          make_context_starter(lambda::bind(fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg)), ctx) \
        , size, no_stack_unwind, return_to_caller).swap(ctx);               \
        start_context(ctx);                                                 \
                                                                            \
        unique_lock<mutex> guard(_m_mtx);                                   \
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));     \
        _m_cond.notify_one();                                               \
    }                                                                       \
// BOOST_MMM_scheduler_add_thread
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTEXT_ARITY), BOOST_MMM_scheduler_add_thread, ~)
#undef BOOST_MMM_scheduler_add_thread
#else
    /**
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
        using contexts::default_stacksize;
        add_thread<Fn &, Args &...>(default_stacksize(), fn, args...);
    }

    /**
     * <b>Effects</b>: Construct context and join to scheduling with specified
     * stack size.
     *
     * <b>Requires</b>: All of functor and arguments are <b>CopyConstructible</b>.
     */
    template <typename Fn, typename... Args>
    void
    add_thread(size_type size, Fn fn, Args... args)
    {
        using contexts::no_stack_unwind;
        using contexts::return_to_caller;

        context_type ctx;
        context_type(
          make_context_starter(lambda::bind(fn, args...), ctx)
        , size, no_stack_unwind, return_to_caller).swap(ctx);
        start_context(ctx);

        unique_lock<mutex> guard(_m_mtx);
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));
        _m_cond.notify_one();
    }
#endif

    /**
     * <b>Effects</b>: Join all <i>user threads</i>.
     *
     * <b>Postcondition</b>: !joinable()
     *
     * <b>Throws</b>: Nothing.
     */
    void
    join_all()
    {
        unique_lock<mutex> guard(_m_mtx);

        while (joinable_nolock())
        {
            _m_status |= _st_join;
            _m_cond.wait(guard);
        }
        _m_status &= ~_st_join;
    }

    /**
     * <b>Returns</b>: true iff any contexts are still not completed. Otherwise
     * false.
     *
     * <b>Throws</b>: Nothing.
     */
    bool
    joinable() const
    {
        unique_lock<mutex> guard(_m_mtx);
        return joinable_nolock();
    }

    /**
     * <b>Returns</b>: Number of <i>kernel threads</i>.
     *
     * <b>Throws</b>: Nothing.
     */
    size_type
    kernel_size() const
    {
        unique_lock<mutex> guard(_m_mtx);
        return _m_kernels.size();
    }

    /**
     * <b>Returns</b>: Number of <i>user threads</i>.
     *
     * <b>Throws</b>: Nothing.
     */
    size_type
    user_size() const
    {
        unique_lock<mutex> guard(_m_mtx);
        return _m_users.size();
    }

private:
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    bool
    joinable_nolock() const
    {
        return _m_users.size() != 0 || _m_runnings != 0;
    }

    template <typename Key, typename Elem>
    struct map_type
    {
        typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
          typename allocator_traits::template rebind_alloc<std::pair<const Key, Elem> >
#else
          typename allocator_type::template rebind<std::pair<const Key, Elem> >::other
#endif
        _alloc_type;

        typedef
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
          unordered_map<Key, Elem, hash<Key>, std::equal_to<Key>, _alloc_type>
#else
          container::map<Key, Elem, std::less<Key>, _alloc_type>
#endif
        type;
    }; // template struct map_type
#endif

    typedef typename map_type<thread::id, thread>::type kernels_type;
    typedef typename strategy_traits::pool_type users_type;

    atomic<int>        _m_status;
    unsigned           _m_runnings;
    mutable mutex      _m_mtx;
    condition_variable _m_cond;
    kernels_type       _m_kernels;
    users_type         _m_users;
}; // template class scheduler

} } // namespace boost::mmm

#endif

