//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_POSIX_UNISTD_HPP
#define BOOST_MMM_IO_POSIX_UNISTD_HPP

#include <cstddef>
#include <sys/types.h>

namespace boost { namespace mmm { namespace io { namespace posix {

/**
 */
ssize_t
read(int, void *, size_t);

/**
 */
ssize_t
write(int, const void *, size_t);

} } } } // namespace boost::mmm:io::posix

#endif

