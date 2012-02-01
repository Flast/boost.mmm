//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_DETAIL_FLAGS_HPP
#define BOOST_MMM_IO_DETAIL_FLAGS_HPP

#include <boost/config.hpp>

namespace boost { namespace mmm { namespace io { namespace detail {

struct polling_events
{
    BOOST_STATIC_CONSTEXPR int in  = 1 << 0;
    BOOST_STATIC_CONSTEXPR int out = 1 << 1;
    BOOST_STATIC_CONSTEXPR int io  = in & out;
};

} } } } // namespace boost::mmm::io::detail

#endif

