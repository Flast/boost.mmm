//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP
#define BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/assert.hpp>
#include <boost/ref.hpp>
#include <boost/mmm/detail/move.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include <boost/container/allocator/allocator_traits.hpp>
#endif
#include <boost/container/vector.hpp>

#include <boost/mmm/detail/thread.hpp>

#include <algorithm>
#if !defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)
#include <boost/tuple/tuple.hpp>
#include <boost/iterator/zip_iterator.hpp>
#define BOOST_MMM_TUPLE ::boost::tuple
#define boost_mmm_make_tuple ::boost::make_tuple
#define boost_mmm_get ::boost::get
#else
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
#include <boost/type_traits/is_convertible.hpp>
#define BOOST_MMM_TUPLE ::boost::fusion::vector
#define boost_mmm_make_tuple ::boost::fusion::make_vector
#define boost_mmm_get ::boost::fusion::at_c
#endif
#include <boost/system/error_code.hpp>
#include <boost/mmm/io/detail/poll.hpp>

namespace boost { namespace mmm { namespace detail {

#if defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)
template <typename IteratorTuple>
struct zip_iterator;

template <typename IteratorTuple>
inline zip_iterator<IteratorTuple>
make_zip_iterator(IteratorTuple t)
{
    return zip_iterator<IteratorTuple>(t);
}
#endif

template <typename Context, typename Alloc>
class async_io_thread
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(async_io_thread)

    typedef io::detail::pollfd pollfd;

    typedef Context context_type;
    typedef Alloc allocator_type;
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<allocator_type> allocator_traits;
#endif

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<context_type *>
#else
      typename allocator_type::template rebind<context_type *>::other
#endif
    ctxptr_alloc_type;

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<pollfd>
#else
      typename allocator_type::template rebind<pollfd>::other
#endif
    pfd_alloc_type;

    typedef container::vector<context_type *, ctxptr_alloc_type> ctxptr_vector;
    typedef container::vector<pollfd, pfd_alloc_type> pollfd_vector;

    thread        _m_th;
    ctxptr_vector _m_ctxptr;
    pollfd_vector _m_pfds;

    struct check_event
    {
        bool
        operator()(BOOST_MMM_TUPLE<const pollfd &, const context_type *> pfd)
        {
            return boost_mmm_get<0>(pfd).revents != 0;
        }
    }; // struct check_event

    void
    exec()
    {
        system::error_code err_code;
        // TODO: check terminate flag
        while (true)
        {
            using io::detail::poll_fds;
            const int ret = poll_fds(_m_pfds.data(), _m_pfds.size(), err_code);
            if (!err_code && 0 < ret)
            {
                typedef
                  BOOST_MMM_TUPLE<
                    typename pollfd_vector::iterator
                  , typename ctxptr_vector::iterator>
                iterator_pair;

                // NOTE: Do not move first descrptor, due to contains pipe to
                // break poller. The range([itr, ends)) contains descrptors:
                // ready to operate I/O request.
                zip_iterator<iterator_pair> itr =
                  std::partition(
                    ++make_zip_iterator(boost_mmm_make_tuple(_m_pfds.begin(), _m_ctxptr.begin()))
                  , make_zip_iterator(boost_mmm_make_tuple(_m_pfds.end(), _m_ctxptr.end()))
                  , check_event());

                // TODO: collect descrptors

                _m_pfds.erase(boost_mmm_get<0>(itr.get_iterator_tuple()), _m_pfds.end());
                _m_ctxptr.erase(boost_mmm_get<1>(itr.get_iterator_tuple()), _m_ctxptr.end());
            }
        }
    }

public:
    async_io_thread()
      : _m_th(&async_io_thread::exec, boost::ref(*this))
    {
        // TODO: create a pipe to break poller
        _m_ctxptr.push_back(static_cast<context_type *>(0));
        BOOST_ASSERT(_m_pfds.size() == 1 && _m_ctxptr.size() == 1);
    }

    async_io_thread(BOOST_RV_REF(async_io_thread) other)
      : _m_th(move(other._m_th))
      , _m_pfds(move(other._m_pfds)) {}

    async_io_thread &
    operator=(BOOST_RV_REF(async_io_thread) other)
    {
        _m_th   = move(other._m_th);
        _m_pfds = move(other._m_pfds);
        return *this;
    }

    void
    join() { _m_th.join(); }

    void
    swap(async_io_thread &other)
    {
        using std::swap;
        swap(_m_th  , other._m_th);
        swap(_m_pfds, other._m_pfds);
    }
}; // template class async_io_thread

template <typename C, typename A>
inline void
swap(async_io_thread<C, A> &l, async_io_thread<C, A> &r)
{
    l.swap(r);
}

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
    // This implementation is not for generic usage.
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
    typedef BOOST_MMM_TUPLE<const IteratorTupleL &, const IteratorTupleR &> zip;
    return fusion::all(
      fusion::zip_view<zip>(
        boost_mmm_make_tuple(l.get_iterator_tuple(), r.get_iterator_tuple()))
    , fusion::make_fused(equal()));
}

#endif

} } } // namespace boost::mmm::detail

#undef boost_mmm_get
#undef boost_mmm_make_tuple
#undef BOOST_MMM_TUPLE

#endif

