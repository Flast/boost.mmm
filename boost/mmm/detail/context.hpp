//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_CONTEXT_HPP
#define BOOST_MMM_DETAIL_CONTEXT_HPP

#include <boost/config.hpp>

#include <cstddef>
#include <algorithm>
#if !defined(BOOST_NO_RVALUE_REFERENCES)
#include <utility>
#   if defined(BOOST_NO_VARIADIC_TEMPLATES)
#   include <boost/preprocessor/cat.hpp>
#   endif
#endif

#include <boost/mmm/detail/move.hpp>
#include <boost/context/context.hpp>

#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#include <boost/preprocessor/arithmetic/add.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/repetition/enum_params.hpp>
#include <boost/preprocessor/repetition/enum_binary_params.hpp>
#endif

namespace boost { namespace mmm { namespace detail {

struct callbacks
{
    struct cb_data
    {
        int   fd;
        short events;
        void  *data;
    }; // struct cb_data
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
}; // struct callbacks

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

    // Inheriting ctors...
//#if defined(BOOST_NO_INHERITING_CTORS)
#if defined(BOOST_NO_VARIADIC_TEMPLATES)
#   if defined(BOOST_NO_RVALUE_REFERENCES)
#define BOOST_MMM_asio_context_forward(unused_z_, n_, unused_data_)
#define BOOST_MMM_asio_context_inheriting_ctor(unused_z_, n_, unused_data_) \
    template <BOOST_PP_ENUM_PARAMS(n_, typename Arg)>       \
    explicit                                                \
    asio_context(BOOST_PP_ENUM_BINARY_PARAMS(n_, Arg, arg)) \
      : _ctx_base_t(BOOST_PP_ENUM_PARAMS(n_, arg)) {}       \
// BOOST_MMM_asio_context_inheriting_ctor
#   else
#define BOOST_MMM_asio_context_forward(unused_z_, n_, unused_data_) \
    std::forward<BOOST_PP_CAT(Arg, n_)>(BOOST_PP_CAT(arg, n_))
#define BOOST_MMM_asio_context_inheriting_ctor(unused_z_, n_, unused_data_) \
    template <BOOST_PP_ENUM_PARAMS(n_, typename Arg)>                       \
    explicit                                                                \
    asio_context(BOOST_PP_ENUM_BINARY_PARAMS(n_, Arg, &&arg))               \
      : _ctx_base_t(BOOST_PP_ENUM(n_, BOOST_MMM_asio_context_forward, ~)) {} \
// BOOST_MMM_asio_context_inheriting_ctor
#   endif
BOOST_PP_REPEAT_FROM_TO(4, BOOST_PP_ADD(BOOST_CONTEXT_ARITY, 5)
, BOOST_MMM_asio_context_inheriting_ctor, ~)
#undef BOOST_MMM_asio_context_inheriting_ctor
#undef BOOST_MMM_asio_context_forward
#else // BOOST_NO_VARIADIC_TEMPLATES
#   if defined(BOOST_NO_RVALUE_REFERENCES)
    template <typename... Args>
    explicit
    asio_context(Args... args)
      : _ctx_base_t(args...) {}
#   else
    template <typename... Args>
    explicit
    asio_context(Args &&... args)
      : _ctx_base_t(std::forward<Args>(args)...) {}
#   endif
#endif
//#else
//    using _ctx_base_t::_ctx_base_t;
//#endif

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
}; // class asio_context

inline void
swap(asio_context &l, asio_context &r)
{
    l.swap(r);
}

} } } // namespace boost::mmm::detail

#endif // BOOST_MMM_DETAIL_CONTEXT_HPP

