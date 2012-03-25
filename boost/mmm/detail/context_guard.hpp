//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_CONTEXT_GUARD_HPP
#define BOOST_MMM_DETAIL_CONTEXT_GUARD_HPP

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#include <boost/config.hpp>
#include <boost/noncopyable.hpp>

#include <boost/mmm/scheduler_traits.hpp>

namespace boost { namespace mmm { namespace detail {

template <typename Scheduler>
class context_guard : private boost::noncopyable
{
    typedef mmm::scheduler_traits<Scheduler> scheduler_traits;
    typedef typename Scheduler::strategy_traits strategy_traits;
    typedef typename strategy_traits::context_type context_type;
    typedef typename strategy_traits::pool_type pool_type;

#if defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS)
    typedef void (context_guard::*unspecified_bool_type)();
    void true_type() {}
#endif

public:
    // The default ctor's costs is not so expensive.
    explicit
    context_guard(scheduler_traits scheduler, strategy_traits strategy)
      : _m_scheduler(scheduler), _m_strategy(strategy)
    {
        _m_strategy.pop_ctx(_m_scheduler).swap(_m_ctx);
    }

    ~context_guard()
    {
        // Back context to pool if it still not finished.
        if (is_suspended())
        {
            _m_strategy.push_ctx(_m_scheduler, move(_m_ctx));
        }
    }

    context_type &
    context() BOOST_NOEXCEPT { return _m_ctx; }

    const context_type &
    context() const BOOST_NOEXCEPT { return _m_ctx; }

#if defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS)
    operator unspecified_bool_type() const BOOST_NOEXCEPT
    {
        return is_suspended() ? &context_guard::true_type : 0;
    }
#else
    explicit
    operator bool() const BOOST_NOEXCEPT
    {
        return is_suspended();
    }
#endif

private:
    bool
    is_suspended() const BOOST_NOEXCEPT
    {
        using fusion::at_c;
        return at_c<0>(context()) && !at_c<0>(context()).is_complete();
    }

    scheduler_traits _m_scheduler;
    strategy_traits  _m_strategy;
    context_type     _m_ctx;
}; // template class context_guard

} } } // namespace boost::mmm::detail

#endif

