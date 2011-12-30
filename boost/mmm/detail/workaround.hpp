//          Copyright Kohei Takahashi 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_WORKAROUND_HPP
#define BOOST_MMM_DETAIL_WORKAROUND_HPP

#include <boost/version.hpp>

// see #6272 in svn.boost.org
//#define BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID

// see #6336 in svn.boost.org
#if BOOST_VERSION < 104900
#   define BOOST_MMM_CONTAINER_BREAKING_EMPLACE_RETURN_TYPE
#endif

// Currently(after Boost 1.48.0), Boost has Move Semantics emulator library: Boost.Move.
// But Boost.Thread implements Move Semantics with original implementations.
//#define BOOST_MMM_THREAD_SUPPORTS_MOVE_BASED_MOVE

#endif

