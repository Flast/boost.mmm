//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_STRATEGY_FIFO_HPP
#define BOOST_MMM_STRATEGY_FIFO_HPP

#include <boost/mmm/detail/workaround.hpp>

#include <boost/preprocessor/stringize.hpp>

#include <boost/assert.hpp>
#include <boost/fusion/include/at.hpp>

#include <boost/intrusive/detail/mpl.hpp>
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

#if !defined(BOOST_MMM_DOXYGEN_INVOKED)
    typedef typename
      BOOST_MMM_ALLOCATOR_REBIND(Allocator)(context_type)
    _allocator_type;
#endif

    typedef container::list<context_type, _allocator_type> pool_type;

    /**
     * <b>Precondition</b>: traits.pool().size() > 0
     *
     * <b>Effects</b>: Pop a context from context pool.
     *
     * <b>Returns</b>: A context which is not a <i>not-a-context</i>.
     */
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

    /**
     * <b>Precondition</b>: ctx is not a <i>not-a-context</i>, !ctx.is_completed()
     *
     * <b>Effects</b>: Push a specified context to context pool.
     */
    template <typename SchedulerTraits>
    void
    push_ctx(SchedulerTraits traits, context_type ctx)
    {
        using fusion::at_c;
        BOOST_ASSERT(at_c<0>(ctx) && !at_c<0>(ctx).is_complete());
        pool_type &pool = traits.pool();

        // Call boost::move via ADL
        pool.push_back(move(ctx));
    }
}; // template struct strategy_traits<strategy::fifo, C, A>

} } // namespace boost::mmm

#endif

