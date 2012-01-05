//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_STRATEGY_FIFO_HPP
#define BOOST_MMM_STRATEGY_FIFO_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/preprocessor/stringize.hpp>

#include <boost/assert.hpp>

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

    // Pre-condition: pool.size() > 0 .
    template <typename SchedulerTraits>
    context_type
    pop_ctx(SchedulerTraits traits)
    {
        pool_type &pool = traits.pool();
        BOOST_ASSERT(pool.size());

        // Call boost::move via ADL
        context_type ctx = move(pool.front());
        pool.pop_front();

        return move(ctx);
    }

    // Pre-condition: Context is not a /not-a-context/ and not completed.
    template <typename SchedulerTraits>
    void
    push_ctx(SchedulerTraits traits, context_type ctx)
    {
        BOOST_ASSERT(ctx && !ctx.is_complete());
        pool_type &pool = traits.pool();

        // Call boost::move via ADL
        pool.push_back(move(ctx));
    }
}; // template class strategy_traits<strategy::fifo>

} } // namespace boost::mmm

#endif

