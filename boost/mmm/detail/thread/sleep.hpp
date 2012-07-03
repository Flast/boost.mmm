//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_THREAD_SLEEP_HPP
#define BOOST_MMM_DETAIL_THREAD_SLEEP_HPP

#include <boost/thread/thread.hpp>

#include <boost/chrono/duration.hpp>
#ifndef BOOST_MMM_THREAD_SUPPORTS_SLEEP_FOR
#include <boost/date_time/posix_time/posix_time_config.hpp>
#endif

namespace boost { namespace mmm { namespace detail { namespace this_thread {

#ifdef BOOST_MMM_THREAD_SUPPORTS_SLEEP_FOR
using boost::this_thread::sleep_for;
#else
template <typename Rep, typename Period>
inline void
sleep_for(chrono::duration<Rep, Period> d)
{
    typedef
#   if defined(BOOST_DATE_TIME_HAS_NANOSECONDS)
      chrono::nanoseconds
#   elif defined(BOOST_DATE_TIME_HAS_MICROSECONDS)
      chrono::microseconds
#   elif defined(BOOST_DATE_TIME_HAS_MILLISECONDS)
      chrono::milliseconds
#   else
      chrono::seconds
#   endif
    fractional_seconds;

    const chrono::hours h = chrono::duration_cast<chrono::hours>(d);
    d -= h;
    const chrono::minutes m = chrono::duration_cast<chrono::minutes>(d);
    d -= m;
    const chrono::seconds s = chrono::duration_cast<chrono::seconds>(d);
    d -= s;
    const fractional_seconds f = chrono::duration_cast<fractional_seconds>(d);

    const posix_time::time_duration xt(h.count(), m.count(), s.count(), f.count());
    boost::this_thread::sleep(xt);
}
#endif

} } } } // namespace boost::mmm::detail::this_thread

#endif

