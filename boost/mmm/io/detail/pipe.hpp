//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_DETAIL_PIPE_HPP
#define BOOST_MMM_IO_DETAIL_PIPE_HPP

#include <boost/config.hpp>

#include <errno.h>
#include <unistd.h>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_convertible.hpp>

#include <boost/utility/swap.hpp>
#include <boost/mmm/detail/move.hpp>

namespace boost { namespace mmm { namespace io { namespace detail {

class pipefd
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(pipefd)

    struct pipe_tag {};

public:
    struct read_tag : public pipe_tag
    {
        BOOST_STATIC_CONSTEXPR int value = 0;
    };
    struct write_tag : public pipe_tag
    {
        BOOST_STATIC_CONSTEXPR int value = 1;
    };

    pipefd()
    {
        if (::pipe(_m_pipe) < 0)
        {
            BOOST_THROW_EXCEPTION(system::system_error(
              system::error_code(errno, system::system_category())));
        }
    }

    pipefd(BOOST_RV_REF(pipefd) other)
    {
        _m_pipe[0] = other._m_pipe[0];
        _m_pipe[1] = other._m_pipe[1];
        other._m_pipe[0] = -1;
        other._m_pipe[1] = -1;
    }

    ~pipefd()
    {
        close<read_tag>();
        close<write_tag>();
    }

    pipefd &
    operator=(BOOST_RV_REF(pipefd) other)
    {
        pipefd(move(other)).swap(*this);
        return *this;
    }

    void
    swap(pipefd &other)
    {
        boost::swap(_m_pipe, other._m_pipe);
    }

    template <typename Tag>
    typename boost::enable_if<boost::is_convertible<Tag, pipe_tag> >::type
    close()
    {
        int &fd = _m_pipe[Tag::value];
        if (0 <= fd)
        {
            ::close(fd);
            fd = -1;
        }
    }

    template <typename Tag>
    typename boost::enable_if<boost::is_convertible<Tag, pipe_tag>, int>::type
    get() const { return _m_pipe[Tag::value]; }

private:
    int _m_pipe[2];
}; // struct pipefd

inline void
swap(pipefd &l, pipefd &r)
{
    l.swap(r);
}

} } } } // namespace boost::mmm::io::detail

#endif // BOOST_MMM_IO_DETAIL_PIPE_HPP

