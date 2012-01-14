#include <iostream>
#include <string>
using namespace std;

#include <boost/mmm/scheduler.hpp>
#include <boost/mmm/strategy.hpp>
#include <boost/mmm/yield.hpp>
namespace mmm = boost::mmm;

//[doc_simple_launching
void f(string which)
{
    cout << "before yield (" << which << ")" << endl;
    mmm::this_ctx::yield();
    cout << "after yield (" << which << ")" << endl;
}

int main()
{
    // Construct scheduler with one kernel thread.
    mmm::scheduler<mmm::strategy::fifo> fifo(1);

    // Add user threads.
    fifo.add_thread(f, "first");
    fifo.add_thread(f, "second");

    // Join all user thread.
    fifo.join_all();
}
//]

