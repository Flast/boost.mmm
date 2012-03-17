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
#include <boost/mmm/detail/iterator/zip_iterator.hpp>
#if !defined(BOOST_MMM_ZIP_ITERATOR_IS_A_INPUT_ITERATOR_CATEGORY)
#include <boost/tuple/tuple.hpp>
#include <boost/fusion/include/boost_tuple.hpp>
#include <boost/fusion/include/at.hpp>
#define BOOST_MMM_TUPLE ::boost::tuple
#define boost_mmm_make_tuple ::boost::make_tuple
#else
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/include/at.hpp>
#define BOOST_MMM_TUPLE ::boost::fusion::vector
#define boost_mmm_make_tuple ::boost::fusion::make_vector
#endif

#include <boost/atomic.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/mmm/detail/locks.hpp>

#include <boost/chrono/duration.hpp>
#include <boost/system/error_code.hpp>
#include <boost/mmm/io/detail/poll.hpp>
#include <boost/mmm/detail/context.hpp>

namespace boost { namespace mmm { namespace detail {

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
            return fusion::at_c<0>(pfd).revents == 0;
        }
    }; // struct check_event

    template <typename Duration>
    int
    polling(unique_lock<mutex> &guard, Duration poll_TO, system::error_code &err_code)
    {
        unique_unlock<mutex> unguard(guard);
        using io::detail::poll_fds;
        return poll_fds(_m_pfds.data(), _m_pfds.size(), poll_TO, err_code);
    }

    template <typename Rep, typename Period>
    void
    exec(chrono::duration<Rep, Period> poll_TO)
    {
        unique_lock<mutex> guard(_m_mtx);

        system::error_code err_code;
        while (!_m_terminate)
        {
            import_pendings();
            const int ret = polling(guard, poll_TO, err_code);

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
          fusion::at_c<1>(itr_tuple), fusion::at_c<1>(end_tuple)
        , phoenix::bind(
            static_cast<eraser>(&ctxact_vector::erase)
          , boost::ref(_m_ctxact)
          , phoenix::placeholders::arg1));
        _m_pfds.erase(fusion::at_c<0>(itr_tuple), fusion::at_c<0>(end_tuple));
        _m_ctxitr.erase(fusion::at_c<1>(itr_tuple), fusion::at_c<1>(end_tuple));
    }

    void
    import_pendings()
    {
        if (_m_pending_ctxs.size() == 0) { return; }

        typedef typename ctxact_vector::iterator iterator;
        iterator itr = --_m_ctxact.end();
        for (iterator i = _m_pending_ctxs.begin(), end = _m_pending_ctxs.end(); i != end; ++i)
        {
            _m_ctxact.push_back(move(*i));
        }
        _m_pending_ctxs.clear();
        for (iterator end = _m_ctxact.end(); ++itr != end; )
        {
            callbacks::cb_data &data = *static_cast<asio_context &>(*itr).data;
            pollfd pfd =
            {
              /*.fd      =*/ data.fd
            , /*.events  =*/ data.events
            , /*.revents =*/ 0
            };
            _m_ctxitr.push_back(itr);
            _m_pfds.push_back(pfd);
        }
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
        _m_pending_ctxs.push_back(move(ctx));
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

#undef boost_mmm_make_tuple
#undef BOOST_MMM_TUPLE

#endif

