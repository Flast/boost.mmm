//          Copyright Kohei Takahashi 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_WORKAROUND_HPP
#define BOOST_MMM_DETAIL_WORKAROUND_HPP

#include <boost/version.hpp>
#include <boost/config.hpp>
#include <boost/thread/detail/config.hpp>

#if BOOST_VERSION < 104800
#   error Boost.MMM requires Boost 1.48.0 or later
#endif

#if !defined(BOOST_NOEXCEPT)
#   define BOOST_NOEXCEPT
#endif

// see #6272 in svn.boost.org
//#define BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID

#if BOOST_VERSION < 104900

// see #6336 in svn.boost.org
#   define BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE

// In 1.48.0, Boost.Container has no allocator traits.
#   define BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS

#endif

// Boost.Container's allocator_traits.hpp will be moved in 1.50.0 release.
#if BOOST_VERSION < 105000
#   define BOOST_MMM_CONTAINER_ALLOCATOR_TRAITS_HEADER "boost/container/allocator/allocator_traits.hpp"
#else
#   define BOOST_MMM_CONTAINER_ALLOCATOR_TRAITS_HEADER "boost/container/allocator_traits.hpp"
#endif

// Currently(after Boost 1.48.0), Boost has Move Semantics emulator library: Boost.Move.
// But Boost.Thread implements Move Semantics with original implementations.
#if defined(BOOST_THREAD_USES_MOVE)
#   define BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE
#endif

#define BOOST_MMM_THREAD_FUTURE ::boost::mmm::BOOST_THREAD_FUTURE

#if defined(BOOST_THREAD_VERSION) && 1 < BOOST_THREAD_VERSION
#   define BOOST_THREAD_SUPPORTS_SLEEP_FOR
#endif

#endif

