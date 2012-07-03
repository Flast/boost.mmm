//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_TASK_HPP
#define BOOST_MMM_DETAIL_TASK_HPP

#include <stdexcept>
#include <boost/throw_exception.hpp>
#include <boost/exception_ptr.hpp>

#include <boost/move/move.hpp>
#include <boost/noncopyable.hpp>

#include <boost/checked_delete.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>

#include <boost/mmm/detail/thread/future.hpp>

namespace boost { namespace mmm { namespace detail {

struct bad_task_call : public std::runtime_error
{
    bad_task_call()
      : std::runtime_error("call to empty boost::mmm::detail::task") {}
};

template <typename>
class packaged_task;

class task
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(task)

protected:
    struct task_impl : private noncopyable
    {
        virtual ~task_impl() {}

        virtual void
        operator()() = 0;
    };

    typedef
      interprocess::unique_ptr<task_impl, checked_deleter<task_impl> >
    task_pointer;

    explicit
    task(task_pointer ptr) BOOST_NOEXCEPT
      : _m_task_ptr(boost::move(ptr)) {}

public:
    task() BOOST_NOEXCEPT {}

    task(BOOST_RV_REF(task) other) BOOST_NOEXCEPT
      : _m_task_ptr(boost::move(other._m_task_ptr)) {}

    task &
    operator=(BOOST_RV_REF(task) other)
    {
        _m_task_ptr = boost::move(other._m_task_ptr);
        return *this;
    }

    bool
    valid() const BOOST_NOEXCEPT
    {
        return static_cast<bool>(_m_task_ptr);
    }

    void
    reset() { task().swap(*this); }

    void
    operator()()
    {
        if (!valid()) { BOOST_THROW_EXCEPTION(bad_task_call()); }

        _m_task_ptr->operator()();
    }

    void
    swap(task &other) BOOST_NOEXCEPT
    {
        _m_task_ptr.swap(other._m_task_ptr);
    }

private:
    // Ignoring swap to packaged_task.
    template <typename R>
    void
    swap(packaged_task<R> &) BOOST_NOEXCEPT;

protected:
    task_pointer _m_task_ptr;
}; // class task

inline void
swap(task &left, task &right) BOOST_NOEXCEPT
{
    left.swap(right);
}

template <typename R>
class packaged_task : public task
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(packaged_task)

private:
    typedef task _base_t;

    template <typename return_type>
    struct packaged_task_aux_ : public _base_t::task_impl
    {
        BOOST_MMM_THREAD_FUTURE<return_type>
        get_future()
        {
            BOOST_MMM_THREAD_FUTURE<return_type> mf(_m_promise.get_future());
            return boost::move(mf);
        }

    protected:
        promise<return_type> _m_promise;
    }; // template struct packaged_task_aux_

    template <typename R_, typename body_type>
    struct packaged_task_impl : public packaged_task_aux_<R_>
    {
        explicit
        packaged_task_impl(body_type f)
          : _m_body(f) {}

        void
        operator()() try
        {
            this->_m_promise.set_value(_m_body());
        }
        catch (...)
        {
            this->_m_promise.set_exception(boost::current_exception());
        }

    private:
        body_type _m_body;
    }; // template struct packaged_task_imp

    template <typename body_type>
    struct packaged_task_impl<void, body_type> : public packaged_task_aux_<void>
    {
        explicit
        packaged_task_impl(body_type f)
          : _m_body(f) {}

        void
        operator()() try
        {
            _m_body();
            this->_m_promise.set_value();
        }
        catch (...)
        {
            this->_m_promise.set_exception(boost::current_exception());
        }

    private:
        body_type _m_body;
    }; // template struct packaged_task_impl<void, >

    // Ignoring swap to not same packaged_task.
    template <typename R_>
    inline void
    swap(packaged_task<R_> &) BOOST_NOEXCEPT;

public:
    typedef R                                    return_type;
    typedef BOOST_MMM_THREAD_FUTURE<return_type> future_type;

    packaged_task() BOOST_NOEXCEPT {}

    packaged_task(BOOST_RV_REF(packaged_task) other) BOOST_NOEXCEPT
      : _base_t(boost::move(static_cast<_base_t &>(other))) {}

    template <typename F>
    packaged_task(F f)
      : _base_t(task_pointer(new packaged_task_impl<return_type, F>(f))) {}

    packaged_task &
    operator=(BOOST_RV_REF(packaged_task) other) BOOST_NOEXCEPT
    {
        _base_t::operator=(boost::move(static_cast<_base_t &>(other)));
        return *this;
    }

    BOOST_MMM_THREAD_FUTURE<return_type>
    get_future()
    {
        if (!valid()) { BOOST_THROW_EXCEPTION(task_moved()); }

        typedef packaged_task_aux_<return_type> aux_type;
        aux_type &aux = static_cast<aux_type &>(*_m_task_ptr);
        return aux.get_future();
    }

    void
    swap(packaged_task &other) BOOST_NOEXCEPT
    {
        _base_t::swap(other);
    }
}; // template class packaged_task

template <typename R>
inline void
swap(packaged_task<R> &left, packaged_task<R> &right) BOOST_NOEXCEPT
{
    left.swap(right);
}

} } } // namespace boost::mmm::detail

#endif

