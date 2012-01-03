//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_STRATEGY_FIFO_HPP
#define BOOST_MMM_STRATEGY_FIFO_HPP

#include <stdexcept>

#include <boost/mmm/detail/workaround.hpp>

#include <boost/preprocessor/stringize.hpp>

#include <boost/intrusive/detail/mpl.hpp>
#include <boost/container/allocator/allocator_traits.hpp>
#include <boost/container/list.hpp>

#include <boost/mmm/strategy_traits.hpp>

namespace boost { namespace mmm {

namespace strategy {

struct fifo {}; // struct fifo

} // namespace boost::mmm::strategy

template <typename Context, typename Allocator>
struct strategy_traits<strategy::fifo, Context, Allocator>
{
    typedef Context context_type;

    typedef container::allocator_traits<Allocator> _allocator_traits;
    typedef typename _allocator_traits::template rebind_alloc<context_type> _allocator_type;

    typedef container::list<context_type, _allocator_type> pool_type;

    // FIXME: should lock to be thread safe
    template <typename SchedulerTraits>
    context_type
    pop_ctx(SchedulerTraits traits)
    {
        pool_type &pool = traits.pool();

        if (!pool.size())
        {
            // XXX: workaround: should block and wait notify
            throw std::out_of_range(__FILE__ "(" BOOST_PP_STRINGIZE(__LINE__) "): workaround");
        }
        // Call boost::move via ADL
        context_type ctx = move(pool.front());
        pool.pop_front();

        return move(ctx);
    }

    // FIXME: should lock to be thread safe
    template <typename SchedulerTraits>
    void
    push_ctx(SchedulerTraits traits, context_type ctx)
    {
        pool_type &pool = traits.pool();

        // Call boost::move using ADL
        pool.push_back(move(ctx));
    }
}; // template class strategy_traits<strategy::fifo>

} } // namespace boost::mmm

#endif

