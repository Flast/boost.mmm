//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_PHOENIX_MOVE_HPP
#define BOOST_MMM_DETAIL_PHOENIX_MOVE_HPP

#include <boost/config.hpp>

#include <boost/move/move.hpp>

#include <boost/mpl/eval_if.hpp>
#include <boost/type_traits/is_lvalue_reference.hpp>
#include <boost/type_traits/remove_reference.hpp>
#if !defined(BOOST_NO_RVALUE_REFERENCES)
#include <boost/type_traits/is_const.hpp>
#include <boost/utility/enable_if.hpp>
#endif

#include <boost/phoenix/function/adapt_callable.hpp>

namespace boost { namespace phoenix {} } // namespace boost::phoenix

namespace boost { namespace mmm { namespace detail {

namespace phoenix_detail {

#if !defined(BOOST_NO_RVALUE_REFERENCES)
template <typename T>
struct rvalue_reference_wrapper
{
    explicit
    rvalue_reference_wrapper(T &value)
      : _m_value(&value) {}

    operator T &&() const { return boost::move(*_m_value); }
private:
    T *_m_value;
}; // template struct rvalue_reference_wrapper

template <typename T>
inline typename disable_if<
  is_const<T>
, rvalue_reference_wrapper<typename remove_reference<T>::type> >::type
rvref(T &&value)
{
    typedef typename remove_reference<T>::type pure_type;
    return rvalue_reference_wrapper<pure_type>(value);
}
#endif

struct move
{
    template <typename>
    struct result; // template struct result

    template <typename This, typename Xvalue>
    struct result<This(Xvalue)>
    {
#ifdef BOOST_NO_RVALUE_REFERENCES
        typedef typename
          mpl::eval_if<
            move_detail::is_rvalue_reference<Xvalue>
          , move_detail::remove_rvalue_reference<Xvalue>
          , remove_reference<Xvalue> >::type
        pure_type;
        typedef BOOST_RV_REF(pure_type) type;
#else
        typedef typename remove_reference<Xvalue>::type pure_type;
        typedef rvalue_reference_wrapper<pure_type> type;
#endif
    }; // template struct result<move(Xvalue)>

#ifdef BOOST_NO_RVALUE_REFERENCES
    template <typename Xvalue>
    typename result<move(Xvalue &)>::type
    operator()(Xvalue &xvalue) const { return boost::move(xvalue); }
#else
    template <typename Xvalue>
    typename result<move(Xvalue &&)>::type
    operator()(Xvalue &&xvalue) const { return rvref(xvalue); }
#endif
}; // struct move

} // namespace boost::mmm::detail::phoenix_detail

namespace phoenix
{

using namespace ::boost::phoenix;
BOOST_PHOENIX_ADAPT_CALLABLE(move, phoenix_detail::move, 1)

} // namespace boost::mmm::detail::phoenix

} } } // namespace boost::mmm::detail

#endif // BOOST_MMM_DETAIL_PHOENIX_MOVE_HPP

