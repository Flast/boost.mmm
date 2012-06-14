//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_LOCKS_HPP
#define BOOST_MMM_DETAIL_LOCKS_HPP

#include <boost/config.hpp>
#include <boost/mmm/detail/workaround.hpp>

#include <boost/ref.hpp>
#include <boost/mmm/detail/move.hpp>
#include <boost/swap.hpp>

#include <boost/thread/locks.hpp>

namespace boost { namespace mmm { namespace detail {

template <typename Mutex>
class unique_unlock
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(unique_unlock)

public:
    // NOTICE: To be exception safe, do not construct using move ctor and
    // do not (un)lock via (un)lock members. unique_lock::(un)lock may throw
    // some exceptions; _m_guard's dtor will be called unexpectedly.
    explicit
    unique_unlock(unique_lock<Mutex> &guard)
      : _m_orig(&guard)
    {
        if (_m_orig)
        {
            _orig().unlock();
            _m_guard.swap(_orig());
        }
    }

    ~unique_unlock()
    {
        if (_m_orig)
        {
            _m_guard.swap(_orig());
            _orig().lock();
        }
    }

    explicit
    unique_unlock(BOOST_RV_REF(unique_unlock) other) BOOST_MMM_NOEXCEPT
    {
        swap(other);
    }

    unique_unlock &
    operator=(BOOST_RV_REF(unique_unlock) other) BOOST_MMM_NOEXCEPT
    {
        unique_unlock(move(other)).swap(*this);
        return *this;
    }

    void
    lock() { _m_guard.lock(); }

    void
    unlock() { _m_guard.unlock(); }

    bool
    owns_lock() const { return _m_guard.owns_lock(); }

    void
    swap(unique_unlock &x)
    {
        using boost::swap;
        _m_guard.swap(x._m_guard);
        swap(_m_orig, x._m_orig);
    }

private:
    unique_lock<Mutex> _m_guard;
    unique_lock<Mutex> *_m_orig;

    unique_lock<Mutex> &
    _orig() const { return *_m_orig; }
}; // template class unique_unlock

template <typename Mutex>
inline void
swap(unique_unlock<Mutex> &a, unique_unlock<Mutex> &b) BOOST_MMM_NOEXCEPT
{
    a.swap(b);
}

} } } // namespace boost::mmm::detail

#endif

