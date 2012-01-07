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

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
#include <boost/assert.hpp>
#include <boost/mmm/detail/unused.hpp>
#endif
#include <boost/ref.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scope_exit.hpp>

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

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/mmm/detail/locks.hpp>

#include <boost/container/allocator/allocator_traits.hpp>
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
#include <boost/unordered_map.hpp>
#include <boost/functional/hash.hpp>
#else
#include <boost/container/flat_map.hpp>
#endif

#include <boost/mmm/detail/current_context.hpp>
#include <boost/mmm/strategy_traits.hpp>
#include <boost/mmm/scheduler_traits.hpp>

namespace boost { namespace mmm {

template <typename Strategy, typename Allocator = std::allocator<int> >
class scheduler : private boost::noncopyable
{
    typedef scheduler this_type;

    friend class context_guard<this_type>;
    friend struct scheduler_traits<this_type>;

    void
    _m_exec()
    {
        typedef typename strategy_traits::context_type context_type;
        typedef context_guard<this_type> context_guard;
        typedef scheduler_traits<this_type> scheduler_traits;

        strategy_traits traits;

        // NOTICE: Do not use _m_terminate as condition: evaling non-atomic type
        // is not safe in non-mutexed statement even if qualified as volatile.
        while (true)
        {
            // Lock until to be able to get least one context.
            unique_lock<mutex> guard(_m_mtx);
            // Check and breaking loop when destructing scheduler.
            while (!_m_terminate && !_m_users.size())
            {
                // TODO: Check interrupts.
                _m_cond.wait(guard);
            }
            if (_m_terminate) { break; }

            context_guard ctx_guard(scheduler_traits(*this), traits);

            {
                using namespace detail;
                unique_unlock<mutex> unguard(guard);
                BOOST_SCOPE_EXIT()
                {
                    current_context::set_current_ctx(0);
                }
                BOOST_SCOPE_EXIT_END
                current_context::set_current_ctx(&ctx_guard.context());

                // TODO: Resume and continue to execute user thread.
            }

            // Notify one when context is not finished.
            if (ctx_guard) { _m_cond.notify_one(); }
        }
    }

public:
    typedef Strategy strategy_type;
    typedef Allocator allocator_type;
    typedef std::size_t size_type;

    explicit
    scheduler(size_type default_size)
      : _m_terminate(false)
    {
        while (default_size--)
        {
            thread th(&scheduler::_m_exec, ref(*this));

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
    }

    // Call std::terminate: works similar to std::thread::~thread.
    // Users should join all user thread before destruct.
    ~scheduler()
    {
        if (joinable()) { std::terminate(); }

        {
            unique_lock<mutex> guard(_m_mtx);
            _m_terminate = true;
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
#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#define BOOST_MMM_scheduler_add_thread(unused_z_, n_, unused_data_)         \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    typename disable_if<is_same<std::size_t, Fn> >::type                    \
    add_thread(Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg))    \
    {                                                                       \
        add_thread<Fn & BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, & BOOST_PP_INTERCEPT)>( \
          contexts::default_stacksize()                                     \
        , fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg));                       \
    }                                                                       \
                                                                            \
    template <typename Fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename Arg)>  \
    void                                                                    \
    add_thread(                                                             \
      std::size_t size                                                      \
    , Fn fn BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, Arg, arg))             \
    {                                                                       \
        typedef typename strategy_traits::context_type context_type;        \
        context_type ctx(                                                   \
          fn BOOST_PP_ENUM_TRAILING_PARAMS(n_, arg)                         \
        , size                                                              \
        , contexts::no_stack_unwind                                         \
        , contexts::return_to_caller);                                      \
                                                                            \
        typedef scheduler_traits<this_type> scheduler_traits;               \
        unique_lock<mutex> guard(_m_mtx);                                   \
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));     \
    }                                                                       \
// BOOST_MMM_scheduler_add_thread
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTEXT_ARITY), BOOST_MMM_scheduler_add_thread, ~)
#undef BOOST_MMM_scheduler_add_thread
#else
    template <typename Fn, typename... Args>
    typename disable_if<is_same<std::size_t, Fn> >::type
    add_thread(Fn fn, Args... args)
    {
        using contexts::default_size;
        add_thread<Fn &, Args &...>(default_size(), fn, args...);
    }

    template <typename Fn, typename... Args>
    void
    add_thread(std::size_t size, Fn fn, Args... args)
    {
        using contexts::no_stack_unwind;
        using contexts::return_to_caller;
        typedef typename strategy_traits::context_type context_type;
        context_type ctx(fn, args..., size, no_stack_unwind, return_to_caller);

        typedef scheduler_traits<this_type> scheduler_traits;
        unique_lock<mutex> guard(_m_mtx);
        strategy_traits().push_ctx(scheduler_traits(*this), move(ctx));
    }
#endif

    // Join all user threads.
    void
    join_all()
    {
        unique_lock<mutex> guard(_m_mtx);
        while (_m_users.size())
        {
            _m_cond.wait(guard);
            _m_cond.notify_one();
        }
    }

    bool
    joinable() const
    {
        unique_lock<mutex> guard(_m_mtx);
        return _m_users.size() != 0;
    }

private:
    typedef container::allocator_traits<allocator_type> allocator_traits;

    typedef
      mmm::strategy_traits<strategy_type, contexts::context, allocator_type>
    strategy_traits;

    template <typename Key, typename Elem>
    struct map_type
    {
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
        typedef const Key _alloc_key_type;
#else
        typedef       Key _alloc_key_type;
#endif

        typedef
          typename allocator_traits::template rebind_alloc<
            std::pair<_alloc_key_type, Elem> >
        _alloc_type;

        typedef
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
          unordered_map<Key, Elem, hash<Key>, std::equal_to<Key>, _alloc_type>
#else
          container::flat_map<Key, Elem, std::less<Key>, _alloc_type>
#endif
        type;
    }; // template class map_type

    typedef typename map_type<thread::id, thread>::type kernels_type;
    typedef typename strategy_traits::pool_type users_type;

    // TODO: Use atomic type. (e.g. Boost.Atomic
    volatile bool      _m_terminate;
    mutable mutex      _m_mtx;
    condition_variable _m_cond;
    kernels_type       _m_kernels;
    users_type         _m_users;
}; // template class scheduler

} } // namespace boost::mmm

#endif

