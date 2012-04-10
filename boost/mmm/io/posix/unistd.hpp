//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_POSIX_UNISTD_HPP
#define BOOST_MMM_IO_POSIX_UNISTD_HPP

#include <cstddef>

#include <boost/optional.hpp>

#include <boost/system/error_code.hpp>
#include <boost/mmm/io/posix/detail/io_callback.hpp>

#include <unistd.h>

namespace boost { namespace mmm { namespace io { namespace posix {

namespace detail {

class read_callback : public posix_callback<ssize_t>
{
    typedef posix_callback<ssize_t> base_type;

    void        *_m_buf;
    std::size_t _m_count;

public:
    explicit
    read_callback(int fd, void *buf, std::size_t count)
      : base_type(base_type::event_type::in, fd)
      , _m_buf(buf), _m_count(count) {}

    virtual void
    operator()()
    {
        set_result(::read(get_fd(), _m_buf, _m_count));
    }
}; // class read_callback

} // namespace boost::mmm::io::posix::detail

/**
 */
inline ssize_t
read(int fd, void *buf, std::size_t count)
{
    using namespace detail;
    read_callback callback(fd, buf, count);

    system::error_code err_code;
    if (count == 0 || callback.check_events(err_code))
    {
        callback();
    }
    else
    {
        yield_blocker_syscall(callback);
    }

    const boost::optional<ssize_t> result = callback.get_result();
    return result != boost::none ? result.get() : -1;
}

namespace detail {

class write_callback : public posix_callback<ssize_t>
{
    typedef posix_callback<ssize_t> base_type;

    const void  *_m_buf;
    std::size_t _m_count;

public:
    explicit
    write_callback(int fd, const void *buf, std::size_t count)
      : base_type(base_type::event_type::out, fd)
      , _m_buf(buf), _m_count(count) {}

    virtual void
    operator()()
    {
        set_result(::write(get_fd(), _m_buf, _m_count));
    }
}; // class write_callback

} // namespace boost::mmm::io::posix::detail

/**
 */
inline ssize_t
write(int fd, const void *buf, std::size_t count)
{
    using namespace detail;
    write_callback callback(fd, buf, count);

    system::error_code err_code;
    if (callback.check_events(err_code))
    {
        callback();
    }
    else
    {
        yield_blocker_syscall(callback);
    }
    const boost::optional<ssize_t> result = callback.get_result();
    return result != boost::none ? result.get() : -1;
}

} } } } // namespace boost::mmm:io::posix

#endif

