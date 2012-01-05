//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_CURRENT_CONTEXT_HPP
#define BOOST_MMM_DETAIL_CURRENT_CONTEXT_HPP

namespace boost { namespace mmm { namespace detail { namespace current_context {

typedef contexts::context * context_type;

void
set_current_ctx(context_type);

context_type
get_current_ctx();

} } } } // namespace boost::mmm::detail::current_context

#endif

