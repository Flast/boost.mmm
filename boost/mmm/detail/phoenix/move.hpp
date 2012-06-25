//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_PHOENIX_MOVE_HPP
#define BOOST_MMM_DETAIL_PHOENIX_MOVE_HPP

#include <boost/move/move.hpp>

#include <boost/mpl/eval_if.hpp>
#include <boost/type_traits/is_lvalue_reference.hpp>
#include <boost/type_traits/remove_reference.hpp>

#include <boost/phoenix/function/adapt_callable.hpp>

namespace boost { namespace phoenix {} } // namespace boost::phoenix

namespace boost { namespace mmm { namespace detail {

namespace phoenix_detail {

struct move
{
    template <typename>
    struct result; // template struct result

    template <typename This, typename Xvalue>
    struct result<This(Xvalue)>
    {
        typedef typename
          boost::mpl::eval_if<
            boost::move_detail::is_rvalue_reference<Xvalue>
          , boost::move_detail::remove_rvalue_reference<Xvalue>
          , boost::remove_reference<Xvalue> >::type
        pure_type;
        typedef BOOST_RV_REF(pure_type) type;
    }; // template struct result<move(Xvalue)>

    template <typename Xvalue>
    typename result<move(Xvalue &)>::type
    operator()(Xvalue &xvalue) { return boost::move(xvalue); }
}; // struct move

} // namespace boost::mmm::detail::phoenix_detail

namespace phoenix
{

using namespace ::boost::phoenix;
BOOST_PHOENIX_ADAPT_CALLABLE(move, phoenix_detail::move, 1)

} // namespace boost::mmm::detail::phoenix

} } } // namespace boost::mmm::detail

#endif // BOOST_MMM_DETAIL_PHOENIX_MOVE_HPP

