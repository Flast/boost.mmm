//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_SCHEDULER_TRAITS_HPP
#define BOOST_MMM_SCHEDULER_TRAITS_HPP

#include <boost/ref.hpp>

namespace boost { namespace mmm {

template <typename Scheduler>
struct scheduler_traits
{
    explicit
    scheduler_traits(Scheduler &sch)
      : _m_scheduler(sch) {}

    typename Scheduler::strategy_traits::pool_type &
    pool()
    {
        return _m_scheduler.get()._m_users;
    }

    boost::reference_wrapper<Scheduler> _m_scheduler;
}; // template class scheduler_traits

} } // namespace boost::mmm

#endif

