#include <boost/mmm/detail/task.hpp>
namespace mmm = boost::mmm;

#include <boost/test/minimal.hpp>

int f() { return 0; }

int test_main(int, char **)
{
    mmm::detail::packaged_task<int> t;
    BOOST_REQUIRE(!t.valid());

    mmm::detail::packaged_task<int> pt(f);
    BOOST_REQUIRE(pt.valid());

    t = boost::move(pt);
    BOOST_REQUIRE(t.valid());
    BOOST_REQUIRE(!pt.valid());

    return 0;
}

