//          Copyright Kohei Takahashi 2011 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_FUTURE_HPP
#define BOOST_MMM_DETAIL_FUTURE_HPP

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#include <boost/thread/future.hpp>

#include <boost/mmm/detail/move.hpp>

namespace boost { namespace mmm {

namespace detail
{
#if defined(BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE)
using boost::BOOST_MMM_THREAD_FUTURE;
#else
template <typename>
class BOOST_MMM_THREAD_FUTURE;
#endif
} // namespace boost::mmm::detail
using detail::BOOST_MMM_THREAD_FUTURE;

#if ! defined(BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE)

namespace detail {

template <typename R>
class BOOST_MMM_THREAD_FUTURE : public boost::BOOST_MMM_THREAD_FUTURE<R>
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(BOOST_MMM_THREAD_FUTURE<R>)

    typedef boost::BOOST_MMM_THREAD_FUTURE<R> _base_t;

public:
    BOOST_MMM_THREAD_FUTURE() {}

    BOOST_MMM_THREAD_FUTURE(BOOST_RV_REF(BOOST_MMM_THREAD_FUTURE) other) BOOST_MMM_NOEXCEPT
      : _base_t(move(static_cast<_base_t &>(other))) {}

    // NOTE: Declare as explicit to avoid ambiguous overload resolution.
    explicit
    BOOST_MMM_THREAD_FUTURE(BOOST_MMM_THREAD_RV_REF(_base_t) other) BOOST_MMM_NOEXCEPT
      : _base_t(other) {}

    BOOST_MMM_THREAD_FUTURE &
    operator=(BOOST_RV_REF(BOOST_MMM_THREAD_FUTURE) other) BOOST_MMM_NOEXCEPT
    {
        _base_t::operator=(move(static_cast<_base_t &>(other)));
        return *this;
    }

    BOOST_MMM_THREAD_FUTURE &
    operator=(BOOST_MMM_THREAD_RV_REF(_base_t) other) BOOST_MMM_NOEXCEPT
    {
        _base_t::operator=(other);
        return *this;
    }
}; // template class BOOST_MMM_THREAD_FUTURE

} // namespace boost::mmm::detail

#endif

} } // namespace boost::mmm

#endif // BOOST_MMM_DETAIL_FUTURE_HPP

