//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_SCHEDULER_TRAITS_HPP
#define BOOST_MMM_SCHEDULER_TRAITS_HPP

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

#include <boost/ref.hpp>
#include <boost/assert.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/mmm/detail/locks.hpp>

namespace boost { namespace mmm {

template <typename Scheduler>
struct scheduler_traits
{
    typedef Scheduler scheduler_type;

    /**
     * <b>Effects</b>: No effects.
     */
    explicit
    scheduler_traits(scheduler_type &sch)
      : _m_scheduler(sch) {}

    /**
     * <b>Precondition</b>: scheduler is not <i>not-in-scheduling</i>.
     *
     * <b>Returns</b>: Context pool.
     */
    typename scheduler_type::strategy_traits::pool_type &
    pool() const BOOST_NOEXCEPT
    {
        BOOST_ASSERT(_m_scheduler.get()._m_data);
        return _m_scheduler.get()._m_data->users;
    }

    /**
     * <b>Effects</b>: Get scheduler locking object.
     *
     * <b>Returns</b>: Locking object.
     */
    unique_lock<mutex>
    get_lock() const
    {
        return unique_lock<mutex>(_m_scheduler.get()._m_data->mtx);
    }

    /**
     * <b>Effects</b>: Notify all suspended scheduler threads without lock.
     */
    void
    notify_all() const
    {
        _m_scheduler.get()._m_data->cond.notify_all();
    }

    /**
     * <b>Effects</b>: Notify a suspended scheduler thread without lock.
     */
    void
    notify_one() const
    {
        _m_scheduler.get()._m_data->cond.notify_one();
    }

    /**
     * <b>Effects</b>: Get scheduler locking object.
     *
     * <b>Returns</b>: Locking object.
     */
    template <typename LockType>
    unique_lock<mutex>
    get_lock(const LockType &lt) const
    {
        return unique_lock<mutex>(_m_scheduler.get()._m_data->mtx, lt);
    }
private:
    boost::reference_wrapper<scheduler_type> _m_scheduler;
}; // template struct scheduler_traits

} } // namespace boost::mmm

#endif

