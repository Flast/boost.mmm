//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_POSIX_UNISTD_HPP
#define BOOST_MMM_IO_POSIX_UNISTD_HPP

#include <cstddef>

#include <boost/mmm/detail/context.hpp>
#include <boost/mmm/detail/current_context.hpp>

#include <boost/optional.hpp>
#include <boost/fusion/include/at.hpp>

#include <boost/mmm/io/detail/check_events.hpp>

#include <unistd.h>

namespace boost { namespace mmm { namespace io { namespace posix {

namespace detail {

template <typename T>
class posix_callback : public mmm::detail::io_callback_base
{
    typedef mmm::detail::io_callback_base base_type;

protected:
    typedef T result_type;

    explicit
    posix_callback(base_type::event_type::type event, int fd)
      : base_type(event), _m_fd(fd), _m_result(0) {}

    void
    set_result(result_type val) { _m_result = val; }

public:
    int
    get_fd() const { return _m_fd; }

    boost::optional<result_type>
    get_result() const { return _m_result; }

    virtual bool
    check_events(system::error_code &err_code) const
    {
        using io::detail::check_events;
        return check_events(get_fd(), get_events(), err_code);
    }

    virtual bool
    done() const { return get_result() != boost::none; }

private:
    int                          _m_fd;
    boost::optional<result_type> _m_result;
}; // template class posix_callback_base

template <typename T>
inline void
yield_blocker_syscall(posix_callback<T> &callback)
{
    using mmm::detail::current_context::get_current_ctx;
    typedef mmm::detail::context_tuple context_tuple;

    if (context_tuple *ctx_tuple = get_current_ctx())
    {
        fusion::at_c<1>(*ctx_tuple) = &callback;
        fusion::at_c<0>(*ctx_tuple).suspend();
    }
    else
    {
        callback();
    }
}

} // namespace boost::mmm::io::posix::detail

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

    if (count == 0)
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
    yield_blocker_syscall(callback);
    const boost::optional<ssize_t> result = callback.get_result();
    return result != boost::none ? result.get() : -1;
}

} } } } // namespace boost::mmm:io::posix

#endif

