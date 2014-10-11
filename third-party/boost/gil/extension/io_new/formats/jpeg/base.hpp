/*
    Copyright 2010 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_DETAIL_JPEG_IO_BASE_HPP
#define BOOST_GIL_EXTENSION_IO_DETAIL_JPEG_IO_BASE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date   2010 \n
///
////////////////////////////////////////////////////////////////////////////////////////
#include <boost/gil/extension/io_new/jpeg_tags.hpp>

namespace boost { namespace gil { namespace detail {

class jpeg_io_base
{

protected:

    jpeg_error_mgr _jerr;
    jmp_buf        _mark;
};


} // namespace detail
} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_DETAIL_JPEG_IO_BASE_HPP
