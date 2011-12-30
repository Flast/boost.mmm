//          Copyright Kohei Takahashi 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_MMM_HPP
#define BOOST_MMM_MMM_HPP

#include <cstddef>
#include <utility>
#include <memory>
#include <functional>

#include <boost/mmm/detail/workaround.hpp>

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
#include <boost/assert.hpp>
#endif
#include <boost/ref.hpp>
#include <boost/functional/hash.hpp>
#include <boost/move/move.hpp>

#include <boost/thread/thread.hpp>
#include <boost/mmm/detail/movable_thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/context/context.hpp>

#include <boost/container/allocator/allocator_traits.hpp>
#include <boost/container/list.hpp>
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
#include <boost/unordered_map.hpp>
#else
#include <boost/container/flat_map.hpp>
#endif

namespace boost { namespace mmm {

template <typename Allocator = std::allocator<int> >
class scheduler;

typedef scheduler<> default_scheduler;

template <typename Allocator>
class scheduler
{
    void
    _m_exec() const
    {
    }

public:
    typedef Allocator allocator_type;
    typedef std::size_t size_type;

    explicit
    scheduler(size_type default_size)
    {
        while (default_size--)
        {
            thread th(&scheduler::_m_exec, ref(*this));

#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            std::pair<typename kernels_type::iterator, bool> r =
#endif
            _m_kernels.emplace(th.get_id(), move(th));
#if !defined(BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE)
            BOOST_ASSERT(r.second);
#endif
        }
    }

private:
    typedef contexts::context context_type;
    typedef container::allocator_traits<allocator_type> allocator_traits;

    typedef
      typename allocator_traits::template rebind_alloc<
        std::pair<const thread::id, thread> >
    thread_id_allocator_type;
    typedef
      typename allocator_traits::template rebind_alloc<context_type>
    context_allocator_type;

    typedef
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
      unordered_map<
#else
      container::flat_map<
#endif
        thread::id
      , thread
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
      , hash<thread::id>
      , std::equal_to<thread::id>
#else
      , std::less<thread::id>
#endif
      , thread_id_allocator_type>
    kernels_type;
    typedef
      container::list<context_type, context_allocator_type>
    users_type;

    mutable shared_mutex _m_sm;
    kernels_type         _m_kernels;
    users_type           _m_users;
}; // template class scheduler

} } // namespace boost::mmm

#endif

