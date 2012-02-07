//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_CONTEXT_HPP
#define BOOST_MMM_DETAIL_CONTEXT_HPP

#include <cstddef>
#include <algorithm>

#include <boost/config.hpp>
#include <boost/move/move.hpp>
#include <boost/context/context.hpp>

namespace boost { namespace mmm { namespace detail {

struct callbacks
{
    struct cb_data
    {
        int  fd;
        int  events;
        void *data;
    };
    void *(*callback)(cb_data *);
    cb_data *data;

    callbacks()
      : callback(0), data(0) {}

    void
    swap(callbacks &other)
    {
        using std::swap;
        swap(callback, other.callback);
        swap(data    , other.data);
    }
};

inline void
swap(callbacks &l, callbacks &r)
{
    l.swap(r);
}

class asio_context
  : public contexts::context
  , public callbacks
{
    typedef contexts::context _ctx_base_t;
    typedef callbacks         _cb_base_t;

    BOOST_MOVABLE_BUT_NOT_COPYABLE(asio_context)

public:
    asio_context() {}

#if defined(BOOST_NO_RVALUE_REFERENCES)
    template <typename Fn>
    asio_context(Fn fn, std::size_t size
    , contexts::flag_unwind_t do_unwind
    , contexts::flag_return_t do_return)
      : _ctx_base_t(fn, size, do_unwind, do_return) {}

    template <typename Fn, typename Alloc>
    asio_context(Fn fn, std::size_t size
    , contexts::flag_unwind_t do_unwind
    , contexts::flag_return_t do_return
    , const Alloc &alloc)
      : _ctx_base_t(fn, size, do_unwind, do_return, alloc) {}
#endif
    template <typename Fn>
    asio_context(BOOST_RV_REF(Fn) fn, std::size_t size
    , contexts::flag_unwind_t do_unwind
    , contexts::flag_return_t do_return)
      : _ctx_base_t(boost::move(fn), size, do_unwind, do_return) {}

    template <typename Fn, typename Alloc>
    asio_context(BOOST_RV_REF(Fn) fn, std::size_t size
    , contexts::flag_unwind_t do_unwind
    , contexts::flag_return_t do_return
    , const Alloc &alloc)
      : _ctx_base_t(boost::move(fn), size, do_unwind, do_return, alloc) {}

    asio_context(BOOST_RV_REF(asio_context) other)
      : _ctx_base_t(boost::move(static_cast<_ctx_base_t &>(other)))
      , _cb_base_t(other) {}

    asio_context &
    operator=(BOOST_RV_REF(asio_context) other)
    {
        asio_context(boost::move(other)).swap(*this);
        return *this;
    }

    void
    swap(asio_context &other)
    {
        _ctx_base_t::swap(other);
        _cb_base_t::swap(other);
    }
};

inline void
swap(asio_context &l, asio_context &r)
{
    l.swap(r);
}

} } } // namespace boost::mmm::detail

#endif // BOOST_MMM_DETAIL_CONTEXT_HPP

