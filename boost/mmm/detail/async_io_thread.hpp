//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP
#define BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/assert.hpp>
#include <boost/ref.hpp>
#include <boost/mmm/detail/move.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include <boost/container/allocator/allocator_traits.hpp>
#endif
#include <boost/container/vector.hpp>

#include <boost/mmm/detail/thread.hpp>

#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include <boost/iterator/zip_iterator.hpp>
#include <boost/system/error_code.hpp>
#include <boost/mmm/io/detail/poll.hpp>

namespace boost { namespace mmm { namespace detail {

template <typename Context, typename Alloc>
class async_io_thread
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(async_io_thread)

    typedef Context context_type;
    typedef Alloc allocator_type;
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<allocator_type> allocator_traits;
#endif

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<context_type *>
#else
      typename allocator_type::template rebind<context_type *>::other
#endif
    ctxptr_alloc_type;

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<io::detail::pollfd>
#else
      typename allocator_type::template rebind<io::detail::pollfd>::other
#endif
    pfd_alloc_type;

    typedef container::vector<context_type *, ctxptr_alloc_type> ctxptr_vector;
    typedef container::vector<io::detail::pollfd, pfd_alloc_type> pollfd_vector;

    thread        _m_th;
    ctxptr_vector _m_ctxptr;
    pollfd_vector _m_pfds;

    struct check_event
    {
        bool
        operator()(boost::tuple<const io::detail::pollfd &, const context_type *> pfd)
        {
            return boost::get<0>(pfd).revents != 0;
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
                typedef
                  boost::tuple<
                    typename pollfd_vector::iterator
                  , typename ctxptr_vector::iterator>
                iterator_pair;

                // NOTE: Do not move first descrptor, due to contains pipe to
                // break poller. The range([itr, ends)) contains descrptors:
                // ready to operate I/O request.
                using std::partition;
                zip_iterator<iterator_pair> itr =
                  partition(
                    ++make_zip_iterator(boost::make_tuple(_m_pfds.begin(), _m_ctxptr.begin()))
                  , make_zip_iterator(boost::make_tuple(_m_pfds.end(), _m_ctxptr.end()))
                  , check_event());

                // TODO: collect descrptors

                _m_pfds.erase(boost::get<0>(itr.get_iterator_tuple()), _m_pfds.end());
                _m_ctxptr.erase(boost::get<1>(itr.get_iterator_tuple()), _m_ctxptr.end());
            }
        }
    }

public:
    async_io_thread()
      : _m_th(&async_io_thread::exec, boost::ref(*this))
    {
        // TODO: create a pipe to break poller
        _m_ctxptr.push_back(0);
        BOOST_ASSERT(_m_pfds.size() == 1 && _m_ctxptr.size() == 1);
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

template <typename C, typename A>
inline void
swap(async_io_thread<C, A> &l, async_io_thread<C, A> &r)
{
    l.swap(r);
}

} } } // namespace boost::mmm::detail

#endif

