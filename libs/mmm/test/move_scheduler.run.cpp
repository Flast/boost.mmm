#include <boost/chrono/duration.hpp>
namespace chrono = boost::chrono;
#include <boost/mmm/scheduler.hpp>
#include <boost/mmm/strategy/fifo.hpp>
namespace mmm = boost::mmm;

#include <boost/test/minimal.hpp>

typedef mmm::scheduler<mmm::strategy::fifo> scheduler;

scheduler foo()
{
    scheduler s(1, chrono::milliseconds(10));
    BOOST_REQUIRE(s.kernel_size() == 1);
    return move(s);
}

int test_main(int, char **)
{
    scheduler s1 = foo();
    BOOST_REQUIRE(s1.kernel_size() == 1);

    scheduler s2 = move(s1);
    BOOST_REQUIRE(s2.kernel_size() == 1);
    return 0;
}

