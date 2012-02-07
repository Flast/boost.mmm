//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_POSIX_UNISTD_HPP
#define BOOST_MMM_IO_POSIX_UNISTD_HPP

#include <cstddef>
#include <utility>

#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/at_c.hpp>

#include <boost/mmm/detail/context.hpp>
#include <boost/mmm/detail/current_context.hpp>
#include <boost/mmm/io/detail/poll.hpp>

#include <unistd.h>

namespace boost { namespace mmm { namespace io { namespace posix {

namespace detail {

template <typename Impl>
inline typename fusion::result_of::at_c<typename Impl::data, 0>::type
yield_blocker_syscall(int fd, short events, typename Impl::data &rd)
{
    using mmm::detail::current_context::get_current_ctx;
    using mmm::detail::asio_context;

    asio_context::cb_data d =
    {
      /*.fd     =*/ fd
    , /*.events =*/ events
    , /*.data   =*/ &rd
    };
    asio_context *ctx = static_cast<asio_context *>(get_current_ctx());
    ctx->callback     = Impl::impl;
    ctx->data         = &d;
    ctx->suspend();
    return fusion::at_c<0>(rd);
}

} // namespace boost::mmm::io::posix::detail

namespace detail {

struct read_impl
{
    typedef fusion::vector<ssize_t, void *, std::size_t> data;

    static void *
    impl(mmm::detail::asio_context::cb_data *d)
    {
        using fusion::at_c;
        data &rd = *static_cast<data *>(d->data);
        at_c<0>(rd) = ::read(d->fd, at_c<1>(rd), at_c<2>(rd));
        return NULL;
    }
}; // struct read_impl

} // namespace boost::mmm::io::posix::detail

/**
 */
inline ssize_t
read(int fd, void *buf, std::size_t count)
{
    if (count == 0) { return ::read(fd, buf, count); }

    using namespace detail;
    using io::detail::polling_events;

    read_impl::data rd(0, buf, count);
    return yield_blocker_syscall<read_impl>(fd, polling_events::in, rd);
}

namespace detail {

struct write_impl
{
    typedef fusion::vector<ssize_t, const void *, std::size_t> data;

    static void *
    impl(mmm::detail::asio_context::cb_data *d)
    {
        using fusion::at_c;
        data &rd = *static_cast<data *>(d->data);
        at_c<0>(rd) = ::write(d->fd, at_c<1>(rd), at_c<2>(rd));
        return NULL;
    }
}; // struct write_impl

} // namespace boost::mmm::io::posix::detail

/**
 */
inline ssize_t
write(int fd, const void *buf, std::size_t count)
{
    using namespace detail;
    using io::detail::polling_events;
    write_impl::data rd(0, buf, count);
    return yield_blocker_syscall<write_impl>(fd, polling_events::out, rd);
}

} } } } // namespace boost::mmm:io::posix

#endif

