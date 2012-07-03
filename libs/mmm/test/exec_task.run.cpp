#include <boost/mmm/detail/task.hpp>
namespace mmm = boost::mmm;

#include <boost/thread/thread.hpp>
#include <boost/mmm/detail/thread/sleep.hpp>
#include <boost/chrono/duration.hpp>

#include <boost/test/minimal.hpp>

template <typename T>
struct adapt_thread_move : public T
{
    adapt_thread_move(boost::detail::thread_move_t<adapt_thread_move> atm)
      : T(boost::move(static_cast<T &>(*atm))) {}

    adapt_thread_move &
    operator=(boost::detail::thread_move_t<adapt_thread_move> atm)
    {
        T::operator=(boost::move(static_cast<T &>(*atm)));
        return *this;
    }

    boost::detail::thread_move_t<adapt_thread_move> move()
    {
        return boost::detail::thread_move_t<adapt_thread_move>(*this);
    }

    explicit
    adapt_thread_move(T &obj)
      : T(boost::move(obj)) {}
};

int nonthrow_task()
{
    boost::this_thread::yield();
    mmm::detail::this_thread::sleep_for(boost::chrono::seconds(1));
    return 1;
}

int test_nonthrow()
{
    mmm::detail::packaged_task<int> nt_pt(nonthrow_task);
    mmm::detail::packaged_task<int>::future_type nt_f = nt_pt.get_future();
    boost::thread th(adapt_thread_move<mmm::detail::packaged_task<int> >(nt_pt).move());
    BOOST_REQUIRE(nt_f.get() == 1);
    th.join();

    return 0;
}

void throw_task()
{
    boost::this_thread::yield();
    throw std::runtime_error("catched implicitly");
}

int test_throw()
{
    mmm::detail::packaged_task<void> t_pt(throw_task);
    mmm::detail::packaged_task<void>::future_type t_f = t_pt.get_future();
    boost::thread th(adapt_thread_move<mmm::detail::packaged_task<void> >(t_pt).move());
    try
    {
        t_f.get();
        BOOST_FAIL("unexpected reaching");
        return -1;
    }
    catch (std::runtime_error &)
    {
        th.join();
    }
    catch (...)
    {
        BOOST_FAIL("unexpected exception type");
        return -2;
    }

    return 0;
}

int test_main(int, char **)
{
    if (const int ret = test_nonthrow()) { return ret; }
    if (const int ret = test_throw()) { return ret; }

    return 0;
}

