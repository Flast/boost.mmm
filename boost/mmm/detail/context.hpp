//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_CONTEXT_HPP
#define BOOST_MMM_DETAIL_CONTEXT_HPP

#include <boost/mmm/detail/move.hpp>
#include <boost/context/context.hpp>

#include <stdexcept>
#include <boost/throw_exception.hpp>

#include <boost/utility/swap.hpp>

#include <boost/mmm/io/detail/poll.hpp>

#include <boost/fusion/include/adapt_struct.hpp>

namespace boost { namespace mmm { namespace detail {

class io_callback_base
{
protected:
    typedef io::detail::polling_events event_type;

    explicit
    io_callback_base(event_type::type event)
      : _m_event(event) {}

    event_type::type
    get_events() const { return _m_event; }

public:
    virtual
    ~io_callback_base() {}

    virtual void
    operator()() = 0;

    virtual bool
    check_events(system::error_code &) const = 0;

    virtual bool
    done() const = 0;

    virtual bool
    is_aggregatable() const { return false; }

    virtual pollfd
    get_pollfd() const
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("non-supported operation"));
    }

private:
    event_type::type _m_event;
}; // class io_callback_base

struct context_tuple
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(context_tuple)

public:
    typedef contexts::context context_type;

    context_tuple() : _m_ctx(), _m_io_callback() {}

    explicit
    context_tuple(BOOST_RV_REF(context_type) ctx, io_callback_base *callback)
      : _m_ctx(move(ctx)), _m_io_callback(callback) {}

    context_tuple(BOOST_RV_REF(context_tuple) other)
      : _m_ctx(move(other._m_ctx))
      , _m_io_callback(other._m_io_callback) {}

    context_tuple &
    operator=(BOOST_RV_REF(context_tuple) other)
    {
        context_tuple(move(other)).swap(*this);
        return *this;
    }

    void
    swap(context_tuple &other)
    {
        boost::swap(_m_ctx          , other._m_ctx);
        boost::swap(_m_io_callback, other._m_io_callback);
    }

    context_type     _m_ctx;
    io_callback_base *_m_io_callback;
}; // struct context_tuple

inline void
swap(context_tuple &l, context_tuple &r)
{
    l.swap(r);
}

} } } // namespace boost::mmm::detail

BOOST_FUSION_ADAPT_STRUCT(
  boost::mmm::detail::context_tuple
, (boost::mmm::detail::context_tuple::context_type, _m_ctx)
  (boost::mmm::detail::io_callback_base *         , _m_io_callback)
  )

#endif // BOOST_MMM_DETAIL_CONTEXT_HPP

