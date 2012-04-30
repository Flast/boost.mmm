//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_POSIX_DETAIL_IO_CALLBACK_HPP
#define BOOST_MMM_IO_POSIX_DETAIL_IO_CALLBACK_HPP

#include <boost/mmm/detail/context.hpp>
#include <boost/mmm/detail/current_context.hpp>

#include <boost/optional.hpp>
#include <boost/fusion/include/at.hpp>

#include <boost/mmm/io/detail/check_events.hpp>

namespace boost { namespace mmm { namespace io { namespace posix { namespace detail {

template <typename T>
class posix_callback : public mmm::detail::io_callback_base
{
    typedef mmm::detail::io_callback_base base_type;

protected:
    typedef T result_type;

    explicit
    posix_callback(base_type::event_type::type event, int fd)
      : base_type(event), _m_fd(fd) {}

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
        return check_events(get_fd(), get_events(), err_code) & get_events();
    }

    virtual bool
    done() const { return get_result() != boost::none; }

    virtual bool
    is_aggregatable() const { return true; }

    virtual pollfd
    get_pollfd() const
    {
        pollfd pfd =
        {
          /*.fd      =*/ get_fd()
        , /*.events  =*/ get_events()
        , /*.revents =*/ 0
        };
        return pfd;
    }

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
        fusion::at_c<0>(*ctx_tuple).jump();
    }
    else
    {
        callback();
    }
}

} } } } } // namespace boost::mmm::io::posix::detail

#endif // BOOST_MMM_IO_POSIX_DETAIL_IO_CALLBACK_HPP

