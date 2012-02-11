//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP
#define BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/ref.hpp>
#include <boost/mmm/detail/move.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include <boost/container/allocator/allocator_traits.hpp>
#endif
#include <boost/container/vector.hpp>

#include <boost/mmm/detail/thread.hpp>

#include <algorithm>
#include <boost/system/error_code.hpp>
#include <boost/mmm/io/detail/poll.hpp>

namespace boost { namespace mmm { namespace detail {

template <typename Alloc>
class async_io_thread
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(async_io_thread)

    typedef Alloc allocator_type;
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<allocator_type> allocator_traits;
#endif
    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<io::detail::pollfd>
#else
      typename allocator_type::template rebind<io::detail::pollfd>::other
#endif
    pfd_alloc_type;

    typedef container::vector<io::detail::pollfd, pfd_alloc_type> pollfd_vector;

    thread        _m_th;
    pollfd_vector _m_pfds;

    struct check_event
    {
        bool
        operator()(const io::detail::pollfd &pfd)
        {
            return pfd.revents != 0;
        }
    }; // struct check_event

    void
    exec()
    {
        system::error_code err_code;
        // TODO: check terminate flag
        while (true)
        {
            using io::detail::poll_fds;
            const int ret = poll_fds(_m_pfds.data(), _m_pfds.count(), err_code);
            if (!err_code && 0 < ret)
            {
                // NOTE: Do not move first descrptor, due to contains pipe to
                // break poller. The range([itr, ends)) contains descrptors:
                // ready to operate I/O request.
                using std::partition;
                typename pollfd_vector::iterator itr =
                  partition(++_m_pfds.begin(), _m_pfds.end(), check_event());

                // TODO: collect descrptors

                _m_pfds.erase(itr, _m_pfds.end());
            }
        }
    }

public:
    async_io_thread()
      : _m_th(&async_io_thread::exec, boost::ref(*this))
    {
        // TODO: create a pipe to break poller
    }

    async_io_thread(BOOST_RV_REF(async_io_thread) other)
      : _m_th(move(other._m_th))
      , _m_pfds(move(other._m_pfds)) {}

    async_io_thread &
    operator=(BOOST_RV_REF(async_io_thread) other)
    {
        _m_th   = move(other._m_th);
        _m_pfds = move(other._m_pfds);
        return *this;
    }

    void
    join() { _m_th.join(); }

    void
    swap(async_io_thread &other)
    {
        using std::swap;
        swap(_m_th  , other._m_th);
        swap(_m_pfds, other._m_pfds);
    }
}; // template class async_io_thread

template <typename Alloc>
inline void
swap(async_io_thread<Alloc> &l, async_io_thread<Alloc> &r)
{
    l.swap(r);
}

} } } // namespace boost::mmm::detail

#endif

