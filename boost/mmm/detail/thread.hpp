//          Copyright Kohei Takahashi 2011 - 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_THREAD_HPP
#define BOOST_MMM_DETAIL_THREAD_HPP

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/enum_trailing_params.hpp>
#include <boost/preprocessor/repetition/enum_trailing_binary_params.hpp>
#endif

#include <boost/thread/thread.hpp>

#include <boost/mmm/detail/move.hpp>

namespace boost { namespace mmm {

namespace detail {
#if defined(BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE)
using boost::thread;
#else
class BOOST_THREAD_DECL thread;
#endif
} // namespace boost::mmm::detail
using detail::thread;

namespace detail {

#if !defined(BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE)

class BOOST_THREAD_DECL thread : public boost::thread
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(thread)

    typedef boost::thread _base_t;

    // Ignoring original impls
#if defined(BOOST_NO_RVALUE_REFERENCES)
    using _base_t::operator BOOST_THREAD_RV_REF(boost::thread);
#endif
    using _base_t::move;

public:
    thread() {}

    // Using Boost.Thread's Move Semantics to move.
    thread(BOOST_RV_REF(thread) other)
      : _base_t(other.move()) {}

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#define BOOST_MMM_thread_variadic_ctor(unused_r_, n_, unused_data_) \
  template <typename F BOOST_PP_ENUM_TRAILING_PARAMS(n_, typename A)> \
  explicit                                                          \
  thread(F f BOOST_PP_ENUM_TRAILING_BINARY_PARAMS(n_, A, a))        \
    : _base_t(f BOOST_PP_ENUM_TRAILING_PARAMS(n_, a)) {}            \
// BOOST_MMM_thread_variadic_ctor
    BOOST_PP_REPEAT(10, BOOST_MMM_thread_variadic_ctor, ~)
#undef BOOST_MMM_thread_variadic_ctor
#else
  template <typename F, typename... Args>
  explicit
  thread(F f, Args... args)
    : _base_t(f, args...) {}
#endif

    thread &
    operator=(BOOST_RV_REF(thread) other)
    {
        _base_t::operator=(other.move());
        return *this;
    }
}; // class thread

#endif

} } } // namespace boost::mmm::detail

#endif

