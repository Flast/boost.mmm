//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_DETAIL_CHECK_EVENTS_HPP
#define BOOST_MMM_IO_DETAIL_CHECK_EVENTS_HPP

#include <boost/config.hpp>

#include <boost/mmm/io/detail/poll.hpp>
#include <boost/chrono/duration.hpp>

namespace boost { namespace mmm { namespace io { namespace detail {

inline int
check_events(int fd, int events)
{
    pollfd pfd =
    {
      /*.fd      =*/ fd
    , /*.events  =*/ events
    , /*.revents =*/ 0
    };
    return poll_fds(&pfd, 1, boost::chrono::seconds(0));
}

} } } } // namespace boost::mmm:io::detail

#endif

