#include <boost/mmm/detail/unused.hpp>

#include <boost/test/minimal.hpp>

int test_main(int, char**)
{
    BOOST_MMM_DETAIL_UNUSED(BOOST_FAIL("should not evaluate"));
    return 0;
}

