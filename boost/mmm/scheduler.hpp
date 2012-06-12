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
#include <boost/cstdint.hpp>

#include <boost/assert.hpp>
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

#include <boost/mmm/detail/context.hpp>
#include <boost/mmm/detail/thread/thread.hpp>
#include <boost/mmm/detail/thread/future.hpp>
#include <boost/utility/result_of.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/phoenix/bind/bind_function_object.hpp>

#include <boost/system/error_code.hpp>

#include <boost/atomic.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/mmm/detail/thread/locks.hpp>

#include <boost/checked_delete.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>

#include <boost/chrono/duration.hpp>
#include <boost/mmm/detail/async_io_thread.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include BOOST_MMM_CONTAINER_ALLOCATOR_TRAITS_HEADER
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

#if !defined(BOOST_MMM_SCHEDULER_MAX_ARITY)
#   define BOOST_MMM_SCHEDULER_MAX_ARITY 10
#endif

namespace boost { namespace mmm {

namespace detail {

struct disabling_asio_pool {}; // struct disabling_asio_pool
BOOST_STATIC_CONSTEXPR disabling_asio_pool noasyncpool;

} // namespace boost::mmm::detail

using detail::noasyncpool;

template <typename Strategy, typename Allocator = std::allocator<void> >
struct scheduler;

namespace detail {

template <typename SchedulerTraits, typename StrategyTraits, typename Allocator>
struct scheduler_data : private noncopyable
{
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<Allocator> allocator_traits;
#endif

    template <typename Key, typename Elem>
    struct map_type
    {
        typedef std::pair<const Key, Elem> value_type;
        typedef typename
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
          allocator_traits::template rebind_alloc<value_type>
#else
          Allocator::template rebind<value_type>::other
#endif
        alloc_type;

        typedef
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID) \
 && defined(BOOST_UNORDERED_USE_MOVE)
          unordered_map<Key, Elem, hash<Key>, std::equal_to<Key>, alloc_type>
#else
          container::map<Key, Elem, std::less<Key>, alloc_type>
#endif
        type;
    }; // template struct map_type

    typedef typename map_type<thread::id, thread>::type kernels_type;
    typedef typename StrategyTraits::pool_type users_type;

    typedef
      detail::async_io_thread<SchedulerTraits, StrategyTraits, Allocator>
    async_io_thread;
    // XXX: Should use specified allocator.
    typedef
      interprocess::unique_ptr<async_io_thread, checked_deleter<async_io_thread> >
    async_pool_type;

    atomic<int>        status;
    unsigned           runnings;
    mutex              mtx;
    condition_variable cond;
    kernels_type       kernels;
    users_type         users;
    async_pool_type    async_pool;

    template <typename Rep, typename Period>
    scheduler_data(SchedulerTraits scheduler_traits, chrono::duration<Rep, Period> poll_TO)
      : status(0), runnings(0)
      , async_pool(new async_io_thread(scheduler_traits, StrategyTraits(), poll_TO)) {}

    explicit
    scheduler_data(disabling_asio_pool)
      : status(0), runnings(0) {}
}; // template struct scheduler_data

} // namespace boost::mmm::detail

template <typename Strategy, typename Allocator>
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

    typedef detail::scheduler_data<scheduler_traits, strategy_traits, allocator_type> scheduler_data;
    typedef typename scheduler_data::kernels_type kernels_type;
    typedef typename scheduler_data::users_type users_type;

public:
    typedef typename strategy_traits::context_type context_type;

private:
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    void
    _m_jump_context(unique_lock<mutex> &guard, scheduler_data &data, context_type &ctx)
    {
        using namespace detail;
        unique_unlock<mutex> unguard(guard);

        io_callback_base *&callback = fusion::at_c<1>(ctx);
        if (callback)
        {
            BOOST_ASSERT(!callback->done());
            if (!data.async_pool || !callback->is_aggregatable())
            {
                system::error_code err_code;
                if (callback->check_events(err_code))
                {
                    // No events were occured.
                    return;
                }
            }
            callback->operator()();
            if (callback->done()) { callback = initialized_value; }
        }

        current_context::set_current_ctx(&ctx);
        fusion::at_c<0>(ctx).jump();
        current_context::set_current_ctx(0);

        if (data.async_pool && callback && callback->is_aggregatable())
        {
            data.async_pool->push_ctx(move(ctx));
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
            _m_jump_context(guard, data, ctx_guard.context());
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
    template <typename R, typename = void>
    struct context_starter;

    template <typename T>
    static BOOST_MMM_THREAD_FUTURE<T>
    start_context(context_type &ctx)
    {
        using namespace detail;

        current_context::set_current_ctx(&ctx);
        BOOST_MMM_THREAD_FUTURE<T> f(reinterpret_cast<promise<T> *>(fusion::at_c<0>(ctx).jump())->get_future());
        current_context::set_current_ctx(0);
        return move(f);
    }

    void
    _m_construct_thread_pool(const int default_count)
    {
        BOOST_ASSERT(_m_data);

        for (int cnt = 0; cnt < default_count; ++cnt)
        {
            thread th(&scheduler::_m_exec, boost::ref(*this), boost::ref(*_m_data));

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            std::pair<typename kernels_type::iterator, bool> r =
#endif
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
#endif

public:
    /**
     * <b>Effects</b>: Move internal scheduler datas from the other. And the
     * other becomes <i>not-in-scheduling</i>.
     *
     * <b>Throws</b>: Nothing.
     */
    scheduler(BOOST_RV_REF(scheduler) other) BOOST_MMM_NOEXCEPT
      : _m_data(move(other._m_data)) {}

    /**
     * <b>Effects</b>: Construct with specified count <i>kernel-threads</i>
     * and poller timeout.
     *
     * <b>Throws</b>: std::invalid_argument (wrapped by Boost.Exception):
     * if default_count <= 0 .
     *
     * <b>Postcondition</b>: *this is not <i>not-in-scheduling</i>.
     */
    template <typename Rep, typename Period>
    explicit
    scheduler(const int default_count, chrono::duration<Rep, Period> poll_TO)
    {
        if (!(0 < default_count))
        {
            using std::invalid_argument;
            BOOST_THROW_EXCEPTION(invalid_argument("default_count should be > 0"));
        }

        // XXX: Should use specified allocator.
        // Defer initializing.
        _m_data.reset(new scheduler_data(scheduler_traits(*this), poll_TO));
        _m_construct_thread_pool(default_count);
    }

    /**
     * <b>Effects</b>: Construct with specified count <i>kernel-threads</i>.
     *
     * <b>Throws</b>: std::invalid_argument (wrapped by Boost.Exception):
     * if default_count <= 0 .
     *
     * <b>Postcondition</b>: *this is not <i>not-in-scheduling</i>.
     */
    explicit
    scheduler(const int default_count, detail::disabling_asio_pool)
    {
        if (!(0 < default_count))
        {
            using std::invalid_argument;
            BOOST_THROW_EXCEPTION(invalid_argument("default_count should be > 0"));
        }

        _m_data.reset(new scheduler_data(noasyncpool));
        _m_construct_thread_pool(default_count);
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
#define BOOST_MMM_enum_rmref_params(n_, T_) \
    BOOST_PP_ENUM_BINARY_PARAMS(n_, typename remove_reference<T_, >::type BOOST_PP_INTERCEPT)
#define BOOST_MMM_scheduler_add_thread(unused_z_, n_, unused_data_)         \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    typename disable_if<                                                    \
      is_same<size_type, Fn>                                                \
    , BOOST_MMM_THREAD_FUTURE<typename result_of<typename remove_reference<Fn>::type(BOOST_PP_ENUM_PARAMS(n_, Arg))>::type> >::type \
    add_thread(Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg))    \
    {                                                                       \
        BOOST_ASSERT(_m_data);                                              \
        return add_thread<Fn & BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, & BOOST_PP_INTERCEPT)>( \
          ctx::default_stacksize()                                          \
        , fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg));                       \
    }                                                                       \
                                                                            \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    BOOST_MMM_THREAD_FUTURE<typename result_of<typename remove_reference<Fn>::type(BOOST_MMM_enum_rmref_params(n_, Arg))>::type> \
    add_thread(size_type size, Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg)) \
    {                                                                       \
        BOOST_ASSERT(_m_data);                                              \
                                                                            \
        typedef typename remove_reference<Fn>::type fn_type;                \
        typedef typename                                                    \
          result_of<fn_type(BOOST_MMM_enum_rmref_params(n_, Arg))>::type    \
        fn_result_type;                                                     \
                                                                            \
        context_type ctx;                                                   \
        detail::context(                                                    \
          phoenix::bind(                                                    \
            context_starter<fn_result_type>(ctx)                            \
          , fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg))                      \
        , size).swap(fusion::at_c<0>(ctx));                                 \
        BOOST_MMM_THREAD_FUTURE<fn_result_type> f = start_context<fn_result_type>(ctx); \
                                                                            \
        unique_lock<mutex> guard(_m_data->mtx);                             \
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));     \
        _m_data->cond.notify_one();                                         \
        return move(f);                                                     \
    }                                                                       \
// BOOST_MMM_scheduler_add_thread
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_MMM_SCHEDULER_MAX_ARITY), BOOST_MMM_scheduler_add_thread, ~)
#undef BOOST_MMM_scheduler_add_thread
#undef BOOST_MMM_enum_rmref_params
#else
    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Effects</b>: Construct context and join to scheduling with default
     * stack size.
     *
     * <b>Returns</b>: An object of future<typename result_of<Fn(Args...)>::type>.
     *
     * <b>Requires</b>: All of functor and arguments are <b>CopyConstructible</b>.
     */
    template <typename Fn, typename... Args>
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    typename disable_if<
      is_same<size_type, Fn>
    , BOOST_MMM_THREAD_FUTURE<typename result_of<typename remove_reference<Fn>::type(Args...)>::type> >::type
#else
    future<typename result_of<Fn(Args...)>::type>
#endif
    add_thread(Fn fn, Args... args)
    {
        BOOST_ASSERT(_m_data);
        using ctx::default_stacksize;
        return add_thread<Fn &, Args &...>(default_stacksize(), fn, args...);
    }

    /**
     * <b>Precondition</b>: *this is not <i>not-in-scheduling</i>.
     *
     * <b>Effects</b>: Construct context and join to scheduling with specified
     * stack size.
     *
     * <b>Returns</b>: An object of future<typename result_of<Fn(Args...)>::type>.
     *
     * <b>Requires</b>: All of functor and arguments are <b>CopyConstructible</b>.
     */
    template <typename Fn, typename... Args>
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    BOOST_MMM_THREAD_FUTURE<typename result_of<typename remove_reference<Fn>::type(typename remove_reference<Args>::type...)>::type>
#else
    future<typename result_of<Fn(Args...)>::type>
#endif
    add_thread(size_type size, Fn fn, Args... args)
    {
        BOOST_ASSERT(_m_data);

        typedef typename remove_reference<Fn>::type fn_type;
        typedef typename
          result_of<fn_type(typename remove_reference<Args>::type...)>::type
        fn_result_type;

        context_type ctx;
        detail::context(
          phoenix::bind(context_starter<fn_result_type>(ctx), fn, args...)
        , size).swap(fusion::at_c<0>(ctx));
        BOOST_MMM_THREAD_FUTURE<fn_result_type> f = start_context<fn_result_type>(ctx);

        unique_lock<mutex> guard(_m_data->mtx);
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));
        _m_data->cond.notify_one();

        return move(f);
    }
#endif

private:
#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    bool
    joinable_nolock() const BOOST_MMM_NOEXCEPT
    {
        return _m_data->users.size() != 0
          || _m_data->runnings != 0
          || (_m_data->async_pool && _m_data->async_pool->joinable());
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
    join_all() BOOST_MMM_NOEXCEPT
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
    joinable() const BOOST_MMM_NOEXCEPT
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
    kernel_size() const BOOST_MMM_NOEXCEPT
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
    user_size() const BOOST_MMM_NOEXCEPT
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

#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
template <typename Strategy, typename Allocator>
template <typename R, typename>
struct scheduler<Strategy, Allocator>::context_starter
{
    typedef void result_type;

    typedef typename scheduler<Strategy, Allocator>::context_type context_type;
    reference_wrapper<context_type> _m_ctx;

    explicit
    context_starter(context_type &ctx)
      : _m_ctx(ctx) {}

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#define BOOST_MMM_context_starter_op_call(unused_z_, n_, unused_data_)  \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)> \
    void                                                                \
    operator()(Fn &fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, &arg)) const \
    {                                                                   \
        promise<R> p;                                                   \
        fusion::at_c<0>(unwrap_ref(_m_ctx)).jump(&p);                   \
        p.set_value(fn(BOOST_PP_ENUM_PARAMS(n_, arg)));                 \
    }                                                                   \
// BOOST_MMM_context_starter_op_call
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_MMM_SCHEDULER_MAX_ARITY), BOOST_MMM_context_starter_op_call, ~)
#undef BOOST_MMM_context_starter_op_call
#else
    template <typename Fn, typename... Args>
    void
    operator()(Fn &fn, Args &... args) const
    {
        promise<R> p;
        fusion::at_c<0>(unwrap_ref(_m_ctx)).jump(&p);
        p.set_value(fn(args...));
    }
#endif
}; // template struct scheduler::context_starter

template <typename Strategy, typename Allocator>
template <typename Dummy>
struct scheduler<Strategy, Allocator>::context_starter<void, Dummy>
{
    typedef void result_type;

    typedef typename scheduler<Strategy, Allocator>::context_type context_type;
    reference_wrapper<context_type> _m_ctx;

    explicit
    context_starter(context_type &ctx)
      : _m_ctx(ctx) {}

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#define BOOST_MMM_context_starter_op_call(unused_z_, n_, unused_data_)  \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)> \
    void                                                                \
    operator()(Fn &fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, &arg)) const \
    {                                                                   \
        promise<void> p;                                                \
        fusion::at_c<0>(unwrap_ref(_m_ctx)).jump(&p);                   \
        fn(BOOST_PP_ENUM_PARAMS(n_, arg));                              \
        p.set_value();                                                  \
    }                                                                   \
// BOOST_MMM_context_starter_op_call
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_MMM_SCHEDULER_MAX_ARITY), BOOST_MMM_context_starter_op_call, ~)
#undef BOOST_MMM_context_starter_op_call
#else
    template <typename Fn, typename... Args>
    void
    operator()(Fn &fn, Args &... args) const
    {
        promise<void> p;
        fusion::at_c<0>(unwrap_ref(_m_ctx)).jump(&p);
        fn(args...);
        p.set_value();
    }
#endif
}; // template struct scheduler::context_starter
#endif

} } // namespace boost::mmm

#endif

