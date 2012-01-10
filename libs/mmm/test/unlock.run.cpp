#include <boost/test/minimal.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/mmm/detail/locks.hpp>
using namespace boost;
using boost::mmm::detail::unique_unlock;

int test_main(int, char **)
{
    mutex mtx;

    unique_lock<mutex> guard(mtx);
    BOOST_REQUIRE(guard.owns_lock());

    {
    unique_unlock<mutex> unguard(guard);
    BOOST_REQUIRE(!guard.owns_lock());
    BOOST_REQUIRE(!unguard.owns_lock());
    }

    BOOST_REQUIRE(guard.owns_lock());
    return 0;
}

