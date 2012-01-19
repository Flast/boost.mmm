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

namespace boost { namespace mmm {

template <typename Scheduler>
struct scheduler_traits
{
    typedef Scheduler scheduler_type;

    explicit
    scheduler_traits(scheduler_type &sch)
      : _m_scheduler(sch) {}

    /**
     * <b>Precondition</b>: scheduler is <i>in-scheduling</i>.
     *
     * <b>Returns</b>: Context pool.
     */
    typename scheduler_type::strategy_traits::pool_type &
    pool() const BOOST_NOEXCEPT
    {
        BOOST_ASSERT(_m_scheduler.get()._m_data);
        return _m_scheduler.get()._m_data->users;
    }

private:
    boost::reference_wrapper<scheduler_type> _m_scheduler;
}; // template struct scheduler_traits

} } // namespace boost::mmm

#endif

