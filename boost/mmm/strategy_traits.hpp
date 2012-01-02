//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_STRATEGY_TRAITS_HPP
#define BOOST_MMM_STRATEGY_TRAITS_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/config.hpp>

#include <boost/noncopyable.hpp>

namespace boost { namespace mmm {

template <typename Strategy, typename Context, typename Allocator>
struct strategy_traits {}; // template class strategy_traits

namespace strategy {} // namespace boost::mmm::strategy

template <typename Scheduler>
class context_guard : private boost::noncopyable
{
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
    context_guard(Scheduler &scheduler, strategy_traits &traits)
      : _m_scheduler(scheduler), _m_traits(traits)
    {
        _m_traits.pop_ctx(_m_scheduler).swap(_m_ctx);
    }

    ~context_guard()
    {
        // Back context to pool if it still not finished.
        if (is_suspended())
        {
            _m_traits.push_ctx(_m_scheduler, move(_m_ctx));
        }
    }

#if defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS)
    operator unspecified_bool_type() const
    {
        return is_suspended() ? &context_guard::true_type : 0;
    }
#else
    explicit
    operator bool() const
    {
        return is_suspended();
    }
#endif

private:
    bool
    is_suspended()
    {
        return _m_ctx && !_m_ctx.is_complete();
    }

    Scheduler       &_m_scheduler;
    strategy_traits &_m_traits;
    context_type    _m_ctx;
}; // template class context_guard

} } // namespace boost::mmm

#endif

