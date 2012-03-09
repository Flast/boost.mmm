//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP
#define BOOST_MMM_DETAIL_ASYNC_IO_THREAD_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/assert.hpp>
#include <boost/ref.hpp>
#include <boost/noncopyable.hpp>

#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
#include <boost/container/allocator/allocator_traits.hpp>
#endif
#include <boost/container/vector.hpp>
#include <boost/container/list.hpp>

#include <boost/mmm/detail/thread.hpp>

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator/self.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>
#include <boost/phoenix/fusion/at.hpp>
#include <boost/mmm/detail/phoenix/move.hpp>

#include <algorithm>
#if !defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)
#include <boost/tuple/tuple.hpp>
#include <boost/iterator/zip_iterator.hpp>
#include <boost/fusion/include/boost_tuple.hpp>
#include <boost/fusion/include/at.hpp>
#define BOOST_MMM_TUPLE ::boost::tuple
#define boost_mmm_make_tuple ::boost::make_tuple
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
#endif

#include <boost/atomic.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/system/error_code.hpp>
#include <boost/mmm/io/detail/poll.hpp>

namespace boost { namespace mmm { namespace detail {

#if defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)
template <typename IteratorTuple>
struct zip_iterator;

template <typename IteratorTuple>
inline zip_iterator<IteratorTuple>
make_zip_iterator(IteratorTuple iterator_tuple)
{
    return zip_iterator<IteratorTuple>(iterator_tuple);
}
#endif

template <typename SchedulerTraits, typename StrategyTraits, typename Alloc>
class async_io_thread : private noncopyable
{
    typedef io::detail::pollfd pollfd;

    typedef typename StrategyTraits::context_type context_type;
    typedef Alloc allocator_type;
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
    typedef container::allocator_traits<allocator_type> allocator_traits;
#endif

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<context_type>
#else
      typename allocator_type::template rebind<context_type>::other
#endif
    ctxact_alloc_type;
    typedef container::list<context_type, ctxact_alloc_type> ctxact_vector;

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<typename ctxact_vector::iterator>
#else
      typename allocator_type::template rebind<typename ctxact_vector::iterator>::other
#endif
    ctxitr_alloc_type;
    typedef container::vector<typename ctxact_vector::iterator, ctxitr_alloc_type> ctxitr_vector;

    typedef
#if !defined(BOOST_MMM_CONTAINER_HAS_NO_ALLOCATOR_TRAITS)
      typename allocator_traits::template rebind_alloc<pollfd>
#else
      typename allocator_type::template rebind<pollfd>::other
#endif
    pfd_alloc_type;
    typedef container::vector<pollfd, pfd_alloc_type> pollfd_vector;

    struct check_event
    {
        bool
        operator()(BOOST_MMM_TUPLE<const pollfd &, typename ctxact_vector::iterator> pfd)
        {
            return fusion::at_c<0>(pfd).revents != 0;
        }
    }; // struct check_event

    void
    exec()
    {
        system::error_code err_code;
        while (!_m_terminate)
        {
            using io::detail::poll_fds;
            const int ret = poll_fds(_m_pfds.data(), _m_pfds.size(), chrono::milliseconds(10), err_code);

            if (!err_code && 0 < ret)
            {
                typedef
                  BOOST_MMM_TUPLE<
                    typename pollfd_vector::iterator
                  , typename ctxitr_vector::iterator>
                iterator_tuple;

                typedef zip_iterator<iterator_tuple> zipitr;
                const zipitr zipend(boost_mmm_make_tuple(_m_pfds.end(), _m_ctxitr.end()));

                zip_iterator<iterator_tuple> itr =
                  std::partition(
                    zipitr(boost_mmm_make_tuple(_m_pfds.begin(), _m_ctxitr.begin()))
                  , zipend
                  , check_event());

                if (itr != zipend)
                {
                    restore_contexts(itr, zipend);
                    erase<iterator_tuple>(itr, zipend);
                }
            }
        }

        // Cleanup all remained contexts.
        restore_contexts(
          make_zip_iterator(boost_mmm_make_tuple(_m_pfds.begin(), _m_ctxitr.begin()))
        , make_zip_iterator(boost_mmm_make_tuple(_m_pfds.end(), _m_ctxitr.end())));
    }

    template <typename ZipIterator>
    void
    restore_contexts(ZipIterator itr, ZipIterator end)
    {
        unique_lock<mutex> guard(_m_scheduler_traits.get_lock());

        typedef void (StrategyTraits::*push_ctx)(SchedulerTraits, context_type);
        // Restore I/O ready contexts to schedular.
        std::for_each(itr, end
        , phoenix::bind(
            static_cast<push_ctx>(&StrategyTraits::push_ctx)
          , boost::ref(_m_strategy_traits)
          , boost::ref(_m_scheduler_traits)
          , phoenix::move(
              *phoenix::at_c<1>(phoenix::placeholders::arg1))));
    }

    template <typename IteratorTuple, typename ZipIterator>
    void
    erase(ZipIterator itr, ZipIterator end)
    {
        const IteratorTuple &itr_tuple = itr.get_iterator_tuple(),
                            &end_tuple = end.get_iterator_tuple();

        typedef typename ctxact_vector::iterator       ctxact_iterator;
        typedef typename ctxact_vector::const_iterator ctxact_const_iterator;
        typedef ctxact_iterator (ctxact_vector::*eraser)(ctxact_const_iterator);
        // Erase resotred contexts.
        std::for_each(
          fusion::at_c<1>(itr_tuple), fusion::at_c<1>(end_tuple)
        , phoenix::bind(
            static_cast<eraser>(&ctxact_vector::erase)
          , boost::ref(_m_ctxact)
          , phoenix::placeholders::arg1));
        _m_pfds.erase(fusion::at_c<0>(itr_tuple), fusion::at_c<0>(end_tuple));
        _m_ctxitr.erase(fusion::at_c<1>(itr_tuple), fusion::at_c<1>(end_tuple));
    }

public:
    explicit
    async_io_thread(SchedulerTraits scheduler_traits, StrategyTraits strategy_traits)
      : _m_scheduler_traits(scheduler_traits), _m_strategy_traits(strategy_traits)
      , _m_th(&async_io_thread::exec, boost::ref(*this))
      , _m_terminate(false) {}

    ~async_io_thread()
    {
        _m_terminate = true;
        _m_th.join();
    }

private:
    SchedulerTraits _m_scheduler_traits;
    StrategyTraits  _m_strategy_traits;
    thread          _m_th;
    ctxact_vector   _m_ctxact;
    ctxitr_vector   _m_ctxitr;
    pollfd_vector   _m_pfds;
    atomic<bool>    _m_terminate;
}; // template class async_io_thread

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
    typedef BOOST_MMM_TUPLE<const IteratorTupleL &, const IteratorTupleR &> zip;
    return fusion::all(
      fusion::zip_view<zip>(
        boost_mmm_make_tuple(l.get_iterator_tuple(), r.get_iterator_tuple()))
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
#endif

} } } // namespace boost::mmm::detail

#undef boost_mmm_make_tuple
#undef BOOST_MMM_TUPLE

#endif

