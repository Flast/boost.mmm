//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_UNUSED_HPP
#define BOOST_MMM_DETAIL_UNUSED_HPP

#define BOOST_MMM_DETAIL_UNUSED(expr) \
  (true ? (void)0 : (void)expr)

#endif

