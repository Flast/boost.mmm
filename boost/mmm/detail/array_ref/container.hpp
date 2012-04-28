//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_DETAIL_ARRAY_REF_CONTAINER_HPP
#define BOOST_MMM_DETAIL_ARRAY_REF_CONTAINER_HPP

#include <boost/mmm/detail/array_ref.hpp>
#include <boost/container/container_fwd.hpp>

namespace boost { namespace mmm { namespace detail {

template <typename T, typename A>
inline array_ref<T>
make_array_ref(container::vector<T, A> &vec)
{
    return make_array_ref(vec.data(), vec.size());
}

} } } // namespace boost::mmm::detail

#endif

