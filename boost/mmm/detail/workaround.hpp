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

#if defined(BOOST_NOEXCEPT)
#   define BOOST_MMM_NOEXCEPT BOOST_NOEXCEPT
#else
#   define BOOST_MMM_NOEXCEPT
#endif

#define BOOST_MMM_DETAIL_UNUSED(expr) \
  (true ? (void)0 : (void)expr)

#if BOOST_VERSION < 104900

// see #6336 in svn.boost.org
#   define BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE

// In 1.48.0, Boost.Container has no allocator traits.
#   define BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS

#endif

#if BOOST_VERSION < 105000

#   define BOOST_MMM_CONTAINER_ALLOCATOR_TRAITS_HEADER \
      "boost/container/allocator/allocator_traits.hpp"

#   define BOOST_MMM_THREAD_FUTURE unique_future
#   define BOOST_MMM_THREAD_RV_REF(TYPE_) ::boost::detail::thread_move_t<TYPE_>
#   define BOOST_MMM_THREAD_HAS_MEMBER_MOVE

#else

// Boost.Container's allocator_traits.hpp will be moved in 1.50.0 release.
#   define BOOST_MMM_CONTAINER_ALLOCATOR_TRAITS_HEADER \
      "boost/container/allocator_traits.hpp"

#   define BOOST_MMM_THREAD_FUTURE BOOST_THREAD_FUTURE
#   define BOOST_MMM_THREAD_RV_REF(TYPE_) BOOST_THREAD_RV_REF(TYPE_)

// see #6272 in svn.boost.org
#   define BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID

#endif

#if 1 < BOOST_THREAD_VERSION

#   define BOOST_MMM_THREAD_SUPPORTS_SLEEP_FOR

// Currently(after Boost 1.48.0), Boost has Move Semantics emulator library: Boost.Move.
// But Boost.Thread v1 implements Move Semantics with original implementations.
#   if defined(BOOST_THREAD_USES_MOVE)
#       define BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE
#   endif

#endif

#if defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)

#   define BOOST_MMM_ALLOCATOR_REBIND(from_) \
      from_::BOOST_MMM_allocator_rebind_to_
#   define BOOST_MMM_allocator_rebind_to_(to_) template rebind<to_>::other

#else

#   include BOOST_MMM_CONTAINER_ALLOCATOR_TRAITS_HEADER
#   define BOOST_MMM_ALLOCATOR_REBIND(from_) \
      ::boost::container::allocator_traits<from_>::BOOST_MMM_allocator_rebind_to_
#   define BOOST_MMM_allocator_rebind_to_(to_) template rebind_alloc<to_>

#endif

#endif

