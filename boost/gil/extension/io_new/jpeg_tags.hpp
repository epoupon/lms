/*
    Copyright 2007-2008 Christian Henning, Andreas Pokorny, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_JPEG_TAGS_HPP 
#define BOOST_GIL_EXTENSION_IO_JPEG_TAGS_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file               
/// \brief All supported jpeg tags by the gil io extension.
/// \author Christian Henning, Andreas Pokorny, Lubomir Bourdev \n
///         
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

extern "C" {
#include <jpeglib.h>
}

#include "detail/base.hpp"

namespace boost { namespace gil {

/// Defines jpeg tag.
struct jpeg_tag : format_tag {};

/// see http://en.wikipedia.org/wiki/JPEG for reference

/// Defines type for image width property.
struct jpeg_image_width : property_base< JDIMENSION > {};

/// Defines type for image height property.
struct jpeg_image_height : property_base< JDIMENSION > {};

/// Defines type for number of components property.
struct jpeg_num_components : property_base< int > {};

/// Defines type for color space property.
struct jpeg_color_space : property_base< J_COLOR_SPACE > {};

/// Defines type for jpeg quality property.
struct jpeg_quality : property_base< int >
{
    static const type default_value = 100;
};

/// Defines type for data precision property.
struct jpeg_data_precision : property_base< int > {};

/// Defines type for dct ( discrete cosine transformation ) method property.
struct jpeg_dct_method : property_base< J_DCT_METHOD >
{
    static const type slow        = JDCT_ISLOW;
    static const type fast        = JDCT_IFAST;
    static const type floating_pt = JDCT_FLOAT;
    static const type fastest     = JDCT_FASTEST;

    static const type default_value = slow;
};

/// Read information for jpeg images.
///
/// The structure is returned when using read_image_info.
template<>
struct image_read_info< jpeg_tag >
{
    /// The image width.
    jpeg_image_width::type _width;

    /// The image height.
    jpeg_image_height::type _height;

    /// The number of channels.
    jpeg_num_components::type _num_components;

    /// The color space.
    jpeg_color_space::type _color_space;

    /// The width of channel.
    /// I believe this number is always 8 in the case libjpeg is built with 8.
    /// see: http://www.asmail.be/msg0055405033.html
    jpeg_data_precision::type _data_precision;
};

/// Read settings for jpeg images.
///
/// The structure can be used for all read_xxx functions, except read_image_info.
template<>
struct image_read_settings< jpeg_tag > : public image_read_settings_base
{
    /// Default constructor
    image_read_settings<jpeg_tag>()
    : image_read_settings_base()
    , _dct_method( jpeg_dct_method::default_value )
    {}

    /// Constructor
    /// \param top_left   Top left coordinate for reading partial image.
    /// \param dim        Dimensions for reading partial image.
    /// \param dct_method Specifies dct method.
    image_read_settings( const point_t&        top_left
                       , const point_t&        dim
                       , jpeg_dct_method::type dct_method = jpeg_dct_method::default_value
                       )
    : image_read_settings_base( top_left
                              , dim
                              )
    , _dct_method( dct_method )
    {}

    /// The dct ( discrete cosine transformation ) method. 
    jpeg_dct_method::type _dct_method;
};

/// Write information for jpeg images.
///
/// The structure can be used for write_view() function.
template<>
struct image_write_info< jpeg_tag >
{
    /// Constructor
    /// \param quality Defines the jpeg quality.
    image_write_info( const jpeg_quality::type    quality     = jpeg_quality::default_value
                    , const jpeg_dct_method::type dct_method  = jpeg_dct_method::default_value
                    )
    : _quality   ( quality    )
    , _dct_method( dct_method )
    {}

    /// The jpeg quality.
    jpeg_quality::type _quality;

    /// The dct ( discrete cosine transformation ) method. 
    jpeg_dct_method::type _dct_method;
};


} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_JPEG_TAGS_HPP
