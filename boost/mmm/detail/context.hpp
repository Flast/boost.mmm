//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_CONTEXT_HPP
#define BOOST_MMM_DETAIL_CONTEXT_HPP

#include <cstddef>
#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/cstdint.hpp>
#include <boost/atomic.hpp>

#include <boost/ref.hpp>
#include <boost/utility/swap.hpp>
#include <boost/utility/value_init.hpp>

#include <boost/noncopyable.hpp>
#include <boost/mmm/detail/move.hpp>

#include <boost/context/fcontext.hpp>
#include <boost/context/stack_allocator.hpp>
#include <boost/context/stack_utils.hpp>

#include <exception>
#include <stdexcept>
#include <boost/throw_exception.hpp>

#include <boost/function.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>

#include <boost/checked_delete.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>

#include <boost/mmm/io/detail/poll.hpp>

#include <boost/fusion/include/adapt_struct.hpp>

namespace boost { namespace mmm { namespace detail {

struct context_exception : public std::logic_error
{
    context_exception(const std::string &v)
      : std::logic_error(v) {}
}; // struct context_exception

struct context
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(context)

private:
#if defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS)
    typedef void (context::*unspecified_bool_type)();
    void true_type_() {}
#endif

    struct context_data_ : private noncopyable
    {
    private:
        enum status_t
        {
            context_status_none
          , context_status_run
          , context_status_suspend
          , context_status_done
        }; // enum status_t

        static void
        _m_executer(intptr_t this_)
        {
            // WARNING: Do not declare not /trivial-copyable class/ variable in this scope.
            // Due to dtor of any variables will be not called.
            context_data_ &self = *static_cast<context_data_ *>(reinterpret_cast<void *>(this_));

            self.suspend(0, true);
            if (self._m_status != context_status_none)
            {
                try
                {
                    self._m_func();
                }
                catch (...)
                {
                    std::terminate();
                }
            }

            self._m_status = context_status_done;
            ctx::jump_fcontext(&self._m_fc, &self._m_ofc, 0); // return to caller
        }

        template <typename A>
        void
        allocate_stack(std::size_t size, A alloc)
        {
            void * const ptr = alloc.allocate(size);
            BOOST_ASSERT(ptr);

            _m_fc.fc_stack.base  = ptr;
            _m_fc.fc_stack.limit = static_cast<char *>(ptr) - size;

            _m_deallocate = phoenix::bind(&A::deallocate, alloc, ptr, size);
        }

    public:
        context_data_(function<void()> f, std::size_t size)
          : _m_status(context_status_none), _m_fc(initialized_value), _m_func(f)
        {
            allocate_stack(size, ctx::stack_allocator());
            ctx::make_fcontext(&_m_fc, _m_executer);

            resume(reinterpret_cast<intptr_t>(static_cast<void *>(this)), true);
        }

        ~context_data_()
        {
            if (_m_status == context_status_none) { resume(0, true); }
            if (!is_complete()) { std::terminate(); }
            _m_deallocate();
        }

        intptr_t
        suspend(intptr_t v = 0, bool jump_anyway = false)
        {
            if (!jump_anyway)
            {
                if (_m_status != context_status_run)
                {
                    BOOST_THROW_EXCEPTION(context_exception("This context is not running ..."));
                }
                _m_status = context_status_suspend;
            }
            return ctx::jump_fcontext(&_m_fc, &_m_ofc, v);
        }

        intptr_t
        resume(intptr_t v = 0, bool jump_anyway = false)
        {
            if (!jump_anyway)
            {
                if (_m_status != context_status_suspend
                  && _m_status != context_status_none)
                {
                    BOOST_THROW_EXCEPTION(context_exception("This context is not suspending ..."));
                }
                _m_status = context_status_run;
            }
            return ctx::jump_fcontext(&_m_ofc, &_m_fc, v);
        }

        bool
        is_complete() const BOOST_NOEXCEPT
        {
            return _m_status == context_status_done;
        }

    private:
        atomic<status_t> _m_status;
        ctx::fcontext_t  _m_ofc;
        ctx::fcontext_t  _m_fc;
        function<void()> _m_func;
        function<void()> _m_deallocate;
    }; // struct context::context_data_

    template <typename T, typename D = checked_deleter<T> >
    struct unique_ptr_
    {
        typedef interprocess::unique_ptr<T, D> type;
    };

public:
    context() {}

    template <typename F>
    explicit
    context(F f, std::size_t size = ctx::default_stacksize())
      : _m_data(new context_data_(f, size)) {}

    context(BOOST_RV_REF(context) other) BOOST_NOEXCEPT
      : _m_data(move(other._m_data)) {}

    context &
    operator=(BOOST_RV_REF(context) other) BOOST_NOEXCEPT
    {
        context(move(other)).swap(*this);
        return *this;
    }

    intptr_t
    suspend(intptr_t v = 0)
    {
        if (*this) { return _m_data->suspend(v); }
        BOOST_THROW_EXCEPTION(context_exception("This context is not valid"));
    }

    intptr_t
    resume(intptr_t v = 0)
    {
        if (*this) { return _m_data->resume(v); }
        BOOST_THROW_EXCEPTION(context_exception("This context is not valid"));
    }

#if defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS)
    operator unspecified_bool_type() const BOOST_NOEXCEPT
    {
        return _m_data ? &context::true_type_ : 0;
    }
#else
    explicit
    operator bool() const BOOST_NOEXCEPT
    {
        return _m_data;
    }
#endif

    bool
    operator!() const BOOST_NOEXCEPT
    {
        return !static_cast<bool>(*this);
    }

    void
    swap(context &other) BOOST_NOEXCEPT
    {
        boost::swap(_m_data, other._m_data);
    }

    bool
    is_complete() const
    {
        if (*this) { return _m_data->is_complete(); }
        BOOST_THROW_EXCEPTION(context_exception("This context is not valid"));
    }

private:
    unique_ptr_<context_data_>::type _m_data;
}; // struct context

inline void
swap(context &left, context &right) BOOST_NOEXCEPT
{
    left.swap(right);
}

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
    typedef context context_type;

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
    swap(context_tuple &other) BOOST_NOEXCEPT
    {
        boost::swap(_m_ctx          , other._m_ctx);
        boost::swap(_m_io_callback, other._m_io_callback);
    }

    context_type     _m_ctx;
    io_callback_base *_m_io_callback;
}; // struct context_tuple

inline void
swap(context_tuple &l, context_tuple &r) BOOST_NOEXCEPT
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

