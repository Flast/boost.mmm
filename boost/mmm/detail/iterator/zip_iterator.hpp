//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ITERATOR_ZIP_ITERATOR_HPP
#define BOOST_MMM_DETAIL_ITERATOR_ZIP_ITERATOR_HPP

#include <boost/mmm/detail/workaround.hpp>

#if defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)
#include <cstddef>
#include <iterator>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/iterator/iterator_categories.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/transform.hpp>
#include <boost/fusion/include/all.hpp>
#include <boost/fusion/include/zip_view.hpp>
#include <boost/fusion/include/make_fused.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/add_pointer.hpp>
#include <boost/type_traits/add_lvalue_reference.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/type_traits/is_convertible.hpp>
#else
#include <boost/iterator/zip_iterator.hpp>
#endif

namespace boost { namespace mmm { namespace detail {
#if defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)

struct dereference_iterator
{
    template <typename>
    struct result;

    template <typename Iterator>
    struct result<dereference_iterator(Iterator)>
    {
        typedef typename
          boost::remove_reference<
            typename boost::remove_const<Iterator>::type>::type
        pure_iterator_type;

        typedef typename
          iterator_reference<pure_iterator_type>::type
        type;
    }; // template struct result<dereference_iterator(Iterator)>

    template <typename Iterator>
    typename result<dereference_iterator(Iterator)>::type
    operator()(const Iterator &itr) const { return *itr; }
}; // struct dereference_iterator

template <typename Category, typename Traversal>
struct category_traversal_pair
  : public Category, public Traversal {};

template <typename IteratorTuple>
struct zip_iterator_base
{
private:
    // XXX: This implementation is not for generic usage.
    typedef std::random_access_iterator_tag category;
    typedef boost::random_access_traversal_tag traversal;

    typedef typename
      mpl::transform<IteratorTuple, iterator_value<mpl::_1> >::type
    value_type;

    typedef typename
      mpl::transform<value_type, boost::add_pointer<mpl::_1> >::type
    pointer;

    typedef typename
      mpl::transform<value_type, boost::add_lvalue_reference<mpl::_1> >::type
    reference;
public:
    typedef
      std::iterator<
        category_traversal_pair<category, traversal>
      , reference // Use reference as value_type.
      , std::ptrdiff_t
      , pointer
      , reference>
    type;
}; // template struct zip_iterator_base

// This is workaround of Boost.Iterator's zip_iterator. The zip_iterator of
// category is input iterator even though traversal category is grater than
// forward traversal category and not single pass iterator.
// This behavior caused by iterator_facade_default_category of iterator_facade
// using is_reference to detect reference type. Note, zip_iterator::reference
// is tuple of each reference of iterator.
template <typename IteratorTuple>
struct zip_iterator
  : public zip_iterator_base<IteratorTuple>::type
{
private:
    typedef typename
      zip_iterator_base<IteratorTuple>::type
    _base_t;

public:
    zip_iterator() {}

    zip_iterator(IteratorTuple iterator_tuple)
      : _m_iterator_tuple(iterator_tuple) {}

    template <typename OtherIteratorTuple>
    zip_iterator(
      const zip_iterator<OtherIteratorTuple> &other
    , typename boost::enable_if<
        boost::is_convertible<OtherIteratorTuple, IteratorTuple> >::type * = 0)
      : _m_iterator_tuple(other.get_iterator_tuple()) {}

    struct increment_iterator
    {
        template <typename Iterator>
        void
        operator()(Iterator &itr) const { ++itr; }
    }; // struct increment_iterator

    zip_iterator &
    operator++()
    {
        fusion::for_each(_m_iterator_tuple, increment_iterator());
        return *this;
    }

    zip_iterator
    operator++(int)
    {
        zip_iterator itr = *this;
        operator++();
        return itr;
    }

    struct decrement_iterator
    {
        template <typename Iterator>
        void
        operator()(Iterator &itr) const { --itr; }
    }; // struct decrement_iterator

    zip_iterator &
    operator--()
    {
        fusion::for_each(_m_iterator_tuple, decrement_iterator());
        return *this;
    }

    zip_iterator
    operator--(int)
    {
        zip_iterator itr = *this;
        operator--();
        return itr;
    }

    typename _base_t::reference
    operator*() const
    {
        return fusion::transform(
          get_iterator_tuple()
        , dereference_iterator());
    }

    const IteratorTuple &
    get_iterator_tuple() const { return _m_iterator_tuple; }

private:
    IteratorTuple _m_iterator_tuple;
}; // template struct zip_iterator

struct equal
{
    typedef bool result_type;

    template <typename LeftT, typename RightT>
    result_type
    operator()(const LeftT &l, const RightT &r) const
    {
        return l == r;
    }
}; // struct equal

template <typename IteratorTupleL, typename IteratorTupleR>
inline bool
operator==(
  const zip_iterator<IteratorTupleL> &l
, const zip_iterator<IteratorTupleR> &r)
{
    typedef fusion::vector<const IteratorTupleL &, const IteratorTupleR &> zip;
    return fusion::all(
      fusion::zip_view<zip>(
        fusion::make_vector(l.get_iterator_tuple(), r.get_iterator_tuple()))
    , fusion::make_fused(equal()));
}

template <typename IteratorTupleL, typename IteratorTupleR>
inline bool
operator!=(
  const zip_iterator<IteratorTupleL> &l
, const zip_iterator<IteratorTupleR> &r)
{
    return !(l == r);
}

template <typename IteratorTuple>
inline zip_iterator<IteratorTuple>
make_zip_iterator(IteratorTuple iterator_tuple)
{
    return zip_iterator<IteratorTuple>(iterator_tuple);
}

#else

using boost::zip_iterator;
using boost::make_zip_iterator;

#endif
} } } // namespace boost::mmm::detail

#endif

