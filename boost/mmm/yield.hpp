//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_YIELD_HPP
#define BOOST_MMM_YIELD_HPP

#include <boost/mmm/detail/current_context.hpp>

namespace boost { namespace mmm { namespace this_ctx {

inline void
yield()
{
    using namespace detail::current_context;
    if (context_type ctx = get_current_ctx())
    {
        ctx->suspend();
    }
}

} } } // namespace boost::mmm::this_ctx

#endif

