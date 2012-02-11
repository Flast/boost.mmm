//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_DETAIL_CHECK_EVENTS_HPP
#define BOOST_MMM_IO_DETAIL_CHECK_EVENTS_HPP

#include <boost/config.hpp>

#include <boost/mmm/io/detail/poll.hpp>
#include <boost/chrono/duration.hpp>

#include <boost/system/error_code.hpp>

#include <boost/mmm/detail/array_ref.hpp>

namespace boost { namespace mmm { namespace io { namespace detail {

inline int
check_events(int fd, short events, system::error_code &err_code)
{
    pollfd pfd =
    {
      /*.fd      =*/ fd
    , /*.events  =*/ events
    , /*.revents =*/ 0
    };
    using mmm::detail::make_array_ref;
    using boost::chrono::seconds;
    if (poll_fds(make_array_ref(&pfd, 1), seconds(0), err_code) == 1)
    {
        return pfd.revents;
    }
    return 0;
}

} } } } // namespace boost::mmm:io::detail

#endif

