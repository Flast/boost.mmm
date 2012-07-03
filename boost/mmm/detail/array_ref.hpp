//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ARRAY_REF_HPP
#define BOOST_MMM_DETAIL_ARRAY_REF_HPP

#include <cstddef>

#include <boost/assert.hpp>

namespace boost { namespace mmm { namespace detail {

template <typename T>
struct array_ref
{
    typedef T           value_type;
    typedef std::size_t size_type;

    typedef       value_type *       pointer;
    typedef const value_type * const_pointer;
    typedef       value_type &       reference;
    typedef const value_type & const_reference;

    typedef       value_type *       iterator;
    typedef const value_type * const_iterator;

    iterator
    begin() { return data(); }

    const_iterator
    begin() const { return data(); }

    iterator
    end() { return begin() + size(); }

    const_iterator
    end() const { return begin() + size(); }

    size_type
    size() const { return _m_len; }

    pointer
    data() { return _m_ptr; }

    const_pointer
    data() const { return _m_ptr; }

    reference
    operator[](size_type len)
    {
        BOOST_ASSERT(len < size());
        return data()[len];
    }

    const_reference
    operator[](size_type len) const
    {
        BOOST_ASSERT(len < size());
        return data()[len];
    }

private:
    const pointer   _m_ptr;
    const size_type _m_len;

    explicit
    array_ref(pointer ptr, size_type len)
      : _m_ptr(ptr), _m_len(len)
    {
        BOOST_ASSERT(_m_ptr != 0 && _m_len != 0);
    }

public:
    template <typename U>
    friend array_ref<U>
    make_array_ref(U *, typename array_ref<U>::size_type);

    template <typename U, typename array_ref<U>::size_type N>
    friend array_ref<U>
    make_array_ref(U (&)[N]);
}; // template struct array_ref

template <typename U>
inline array_ref<U>
make_array_ref(U *ptr, typename array_ref<U>::size_type len)
{
    return array_ref<U>(ptr, len);
}

template <typename U, typename array_ref<U>::size_type N>
inline array_ref<U>
make_array_ref(U (&arr)[N])
{
    return array_ref<U>(arr, N);
}

} } } // namespace boost::mmm::detail

#endif

