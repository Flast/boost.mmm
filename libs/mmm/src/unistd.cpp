//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <unistd.h>
#include <poll.h>

#include <boost/mmm/yield.hpp>
#include <boost/mmm/io/posix/unistd.hpp>
#include <boost/mmm/detail/current_context.hpp>

namespace boost { namespace mmm { namespace io { namespace posix {

namespace {

bool
not_in_scheduling()
{
    return !detail::current_context::get_current_ctx();
}

int
check_events(int fd, short events, short &revents)
{
    struct pollfd pfd =
    {
      /*.fd      =*/ fd,
      /*.events  =*/ events,
      /*.revents =*/ 0
    };

    const int ret = poll(&pfd, 1, 0);
    revents = pfd.revents;
    return ret;
}

} // anonymous namespace

ssize_t
read(int fd, void *buf, size_t count)
{
    if (count == 0 || not_in_scheduling())
    {
        return ::read(fd, buf, count);
    }

    while (true)
    {
        short ev = 0;
        const int r = check_events(fd, POLLIN, ev);
        if (r != 0 && ev == POLLIN)
        {
            return ::read(fd, buf, count);
        }

        if (r < 0) { return r; }
        this_ctx::yield();
    }
}

ssize_t
write(int fd, const void *buf, size_t count)
{
    if (not_in_scheduling())
    {
        return ::write(fd, buf, count);
    }

    while (true)
    {
        short ev = 0;
        const int r = check_events(fd, POLLOUT, ev);
        if (r != 0 && ev == POLLOUT)
        {
            return ::write(fd, buf, count);
        }

        if (r < 0) { return r; }
        this_ctx::yield();
    }
}

} } } } // namespace boost::mmm:io::posix

