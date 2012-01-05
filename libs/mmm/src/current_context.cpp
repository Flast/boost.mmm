//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <utility>
using namespace std;

#include <boost/mmm/detail/workaround.hpp>

#include <boost/assert.hpp>
#include <boost/mmm/detail/unused.hpp>

#include <boost/thread/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include <boost/context/context.hpp>

#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
#include <boost/unordered_map.hpp>
#else
#include <boost/container/flat_map.hpp>
#endif

#include <boost/mmm/detail/current_context.hpp>

namespace boost { namespace mmm { namespace detail { namespace current_context {

typedef
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
  unordered_map
#else
  container::flat_map
#endif
    <thread::id, context_type>
context_map_type;

namespace {
shared_mutex     _ctxmtx;
context_map_type _ctxmap;
} // anonymous namespace

void
set_current_ctx(context_type ctx)
{
    const thread::id tid = this_thread::get_id();

    {
        shared_lock<shared_mutex> guard(_ctxmtx);
        context_map_type::iterator itr = _ctxmap.find(tid);
        if (itr != _ctxmap.end())
        {
            itr->second = ctx;
            return;
        }
    }

    lock_guard<shared_mutex> guard(_ctxmtx);
    pair<context_map_type::iterator, bool> r =
#if defined(BOOST_MMM_THREAD_SUPPORTS_HASHABLE_THREAD_ID)
      _ctxmap.emplace(tid, ctx);
#else
      _ctxmap.insert(make_pair(tid, ctx));
#endif
    BOOST_ASSERT(r.second);
    BOOST_MMM_DETAIL_UNUSED(r);
}

context_type
get_current_ctx()
{
    const thread::id tid = this_thread::get_id();

    shared_lock<shared_mutex> guard(_ctxmtx);
    context_map_type::const_iterator itr = _ctxmap.find(tid);
    if (itr == _ctxmap.cend()) { return NULL; }
    return itr->second;
}

} } } } // namespace boost::mmm::detail::current_context

