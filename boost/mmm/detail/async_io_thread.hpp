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

#include <boost/container/vector.hpp>
#include <boost/container/list.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>

#include <boost/mmm/detail/thread/thread.hpp>
#include <boost/mmm/detail/context.hpp>

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator/self.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>
#include <boost/phoenix/fusion/at.hpp>
#include <boost/mmm/detail/phoenix/move.hpp>

#include <algorithm>
#include <iterator>
#include <cstddef>
#include <boost/iterator/zip_iterator.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/fusion/include/boost_tuple.hpp>

#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/mmm/detail/thread/locks.hpp>

#include <boost/chrono/duration.hpp>
#ifndef BOOST_MMM_THREAD_SUPPORTS_SLEEP_FOR
#include <boost/date_time/posix_time/posix_time_config.hpp>
#endif
#include <boost/system/error_code.hpp>
#include <boost/mmm/detail/array_ref/container.hpp>
#include <boost/mmm/io/detail/poll.hpp>

namespace boost { namespace mmm { namespace detail {

// This dummy iterator guards unexpected behavior caused by specialized
// std::iterator_traits.
template <typename Category>
struct specialization_guard_iterator
  : public std::iterator<
      Category
    , specialization_guard_iterator<Category>
    , std::ptrdiff_t
    , specialization_guard_iterator<Category>
    , specialization_guard_iterator<Category> >
{
    specialization_guard_iterator
    operator++() const { return *this;}

    specialization_guard_iterator
    operator--() const { return *this; }

    specialization_guard_iterator
    operator*() const { return *this; }

    bool
    operator==(specialization_guard_iterator) const { return true; }

    bool
    operator!=(specialization_guard_iterator) const { return true; }
};

template <typename SchedulerTraits, typename StrategyTraits, typename Alloc>
class async_io_thread : private noncopyable
{
    typedef io::detail::pollfd pollfd;

    typedef typename StrategyTraits::context_type context_type;
    typedef Alloc allocator_type;

    typedef typename
      BOOST_MMM_ALLOCATOR_REBIND(allocator_type)(context_type)
    ctxact_alloc_type;
    typedef container::list<context_type, ctxact_alloc_type> ctxact_vector;

    typedef typename
      BOOST_MMM_ALLOCATOR_REBIND(allocator_type)(typename ctxact_vector::iterator)
    ctxitr_alloc_type;
    typedef container::vector<typename ctxact_vector::iterator, ctxitr_alloc_type> ctxitr_vector;

    typedef typename
      BOOST_MMM_ALLOCATOR_REBIND(allocator_type)(pollfd)
    pfd_alloc_type;
    typedef container::vector<pollfd, pfd_alloc_type> pollfd_vector;

    struct check_event
    {
        struct ignore
        {
            template <typename T>
            ignore(const T &) {}
        };

        bool
        operator()(boost::tuple<ignore, const pollfd &, ignore> pfd) const
        {
            return boost::get<1>(pfd).revents == 0;
        }
    }; // struct check_event

    template <typename Duration>
    int
    polling(Duration poll_TO, system::error_code &err_code)
    {
        if (_m_pfds.size())
        {
            using io::detail::poll_fds;
            return poll_fds(make_array_ref(_m_pfds), poll_TO, err_code);
        }

#ifdef BOOST_MMM_THREAD_SUPPORTS_SLEEP_FOR
        this_thread::sleep_for(poll_TO);
#else
        typedef
#   if defined(BOOST_DATE_TIME_HAS_NANOSECONDS)
          chrono::nanoseconds
#   elif defined(BOOST_DATE_TIME_HAS_MICROSECONDS)
          chrono::microseconds
#   elif defined(BOOST_DATE_TIME_HAS_MILLISECONDS)
          chrono::milliseconds
#   else
          chrono::seconds
#   endif
        fractional_seconds_type;

        const chrono::hours h = chrono::duration_cast<chrono::hours>(poll_TO);
        poll_TO -= h;
        const chrono::minutes m = chrono::duration_cast<chrono::minutes>(poll_TO);
        poll_TO -= m;
        const chrono::seconds s = chrono::duration_cast<chrono::seconds>(poll_TO);
        poll_TO -= s;
        const fractional_seconds_type f = chrono::duration_cast<fractional_seconds_type>(poll_TO);

        this_thread::sleep(posix_time::time_duration(h.count(), m.count(), s.count(), f.count()));
#endif
        return 0;
    }

    template <typename Rep, typename Period>
    void
    exec(chrono::duration<Rep, Period> poll_TO)
    {
        typedef
          specialization_guard_iterator<std::random_access_iterator_tag>
        guard_iterator;

        system::error_code err_code;
        while (!_m_terminate)
        {
            import_pendings();
            const int ret = polling(poll_TO, err_code);

            if (!err_code && 0 < ret)
            {
                typedef
                  boost::tuple<
                    guard_iterator
                  , typename pollfd_vector::iterator
                  , typename ctxitr_vector::iterator>
                iterator_tuple;

                typedef zip_iterator<iterator_tuple> zipitr;
                const zipitr zipend(boost::make_tuple(guard_iterator(), _m_pfds.end(), _m_ctxitr.end()));

                zip_iterator<iterator_tuple> itr =
                  std::partition(
                    zipitr(boost::make_tuple(guard_iterator(), _m_pfds.begin(), _m_ctxitr.begin()))
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
          make_zip_iterator(boost::make_tuple(guard_iterator(), _m_pfds.begin(), _m_ctxitr.begin()))
        , make_zip_iterator(boost::make_tuple(guard_iterator(), _m_pfds.end(), _m_ctxitr.end())));
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
          , phoenix::move(*phoenix::at_c<2>(phoenix::placeholders::arg1))));
        _m_scheduler_traits.notify_all();
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
          boost::get<2>(itr_tuple), boost::get<2>(end_tuple)
        , phoenix::bind(
            static_cast<eraser>(&ctxact_vector::erase)
          , boost::ref(_m_ctxact)
          , phoenix::placeholders::arg1));
        _m_pfds.erase(boost::get<1>(itr_tuple), boost::get<1>(end_tuple));
        _m_ctxitr.erase(boost::get<2>(itr_tuple), boost::get<2>(end_tuple));
    }

    void
    import_pendings()
    {
        typedef typename ctxact_vector::iterator iterator;

        // Should decrement end iterator due to some end iterator uses special
        // version.
        iterator itr = --_m_ctxact.end();
        if (_m_pending_ctxs.size() != 0)
        {
            lock_guard<mutex> guard(_m_mtx);
            boost::move(boost::begin(_m_pending_ctxs), boost::end(_m_pending_ctxs), back_move_inserter(_m_ctxact));
            _m_pending_ctxs.clear();
        }
        for (iterator end = _m_ctxact.end(); ++itr != end; )
        {
            _m_ctxitr.push_back(itr);
            _m_pfds.push_back(fusion::at_c<1>(*itr)->get_pollfd());
        }
        BOOST_ASSERT(_m_ctxact.size() == _m_ctxitr.size());
        BOOST_ASSERT(_m_ctxact.size() == _m_pfds.size());
    }

public:
    template <typename Rep, typename Period>
    explicit
    async_io_thread(SchedulerTraits scheduler_traits, StrategyTraits strategy_traits
    , chrono::duration<Rep, Period> poll_TO)
      : _m_scheduler_traits(scheduler_traits), _m_strategy_traits(strategy_traits)
      , _m_th(&async_io_thread::exec<Rep, Period>, boost::ref(*this), poll_TO)
      , _m_terminate(false) {}

    ~async_io_thread()
    {
        _m_terminate = true;
        _m_th.join();
    }

    void
    push_ctx(context_type ctx)
    {
        lock_guard<mutex> guard(_m_mtx);
        _m_pending_ctxs.push_back(boost::move(ctx));
    }

    bool
    joinable()
    {
        lock_guard<mutex> guard(_m_mtx);
        return (_m_ctxact.size() + _m_pending_ctxs.size()) != 0;
    }

private:
    SchedulerTraits _m_scheduler_traits;
    StrategyTraits  _m_strategy_traits;
    mutex           _m_mtx;
    thread          _m_th;
    ctxact_vector   _m_ctxact;
    ctxitr_vector   _m_ctxitr;
    ctxact_vector   _m_pending_ctxs;
    pollfd_vector   _m_pfds;
    atomic<bool>    _m_terminate;
}; // template class async_io_thread

} } } // namespace boost::mmm::detail

namespace std {

// This is specialized iterator_traits for Boost.Iterator's zip_iterator. The
// zip_iterator of category is input iterator even though traversal category
// is grater than forward traversal category and not single pass iterator.
// This behavior is expected and caused by iterator_facade_default_category of
// iterator_facade using is_reference to detect reference type. Note,
// zip_iterator::reference is tuple of each reference of iterator.
template <typename Category, typename Iterator1, typename Iterator2>
struct iterator_traits<
  boost::zip_iterator<boost::tuple<
    boost::mmm::detail::specialization_guard_iterator<Category>
  , Iterator1
  , Iterator2> > >
{
private:
    typedef
      boost::zip_iterator<boost::tuple<
        boost::mmm::detail::specialization_guard_iterator<Category>
      , Iterator1
      , Iterator2> >
    zip_iterator;

public:
    typedef typename zip_iterator::value_type      value_type;
    typedef typename zip_iterator::reference       reference;
    typedef typename zip_iterator::pointer         pointer;
    typedef typename zip_iterator::difference_type difference_type;
    typedef Category                               iterator_category;
}; // template <> struct iterator_traits<zip_iterator>

} // namespace std

#endif

