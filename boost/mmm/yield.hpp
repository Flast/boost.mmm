//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_YIELD_HPP
#define BOOST_MMM_YIELD_HPP

#include <boost/mmm/detail/context.hpp>
#include <boost/mmm/detail/current_context.hpp>

#include <boost/fusion/include/at.hpp>

namespace boost { namespace mmm { namespace this_ctx {

/**
 * <b>Effects</b>: Yield context execution to others. No effects if current
 * context is not controlled under scheduler.
 */
inline void
yield()
{
    using namespace detail::current_context;
    if (detail::context_tuple *ctx_tuple = get_current_ctx())
    {
        fusion::at_c<0>(*ctx_tuple).jump();
    }
}

} } } // namespace boost::mmm::this_ctx

#endif

