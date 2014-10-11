/*
    Copyright 2007-2008 Christian Henning, Andreas Pokorny, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_IMAGE_READ_INFO_HPP
#define BOOST_GIL_EXTENSION_IO_IMAGE_READ_INFO_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning, Andreas Pokorny, Lubomir Bourdev \n
///
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/type_traits/is_base_and_derived.hpp>
#include <boost/mpl/and.hpp>
#include <boost/utility/enable_if.hpp>

#include "base.hpp"
#include "io_device.hpp"
#include "path_spec.hpp"

namespace boost{ namespace gil {

/// \ingroup IO

/// \brief Returns the image info. Image info is file format specific.
/// \param file      It's a device. Must satisfy is_input_device metafunction.
/// \param settings  Specifies read settings depending on the image format.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename Device
        , typename FormatTag
        >
inline
image_read_info< FormatTag >
read_image_info( Device&                                 file
               , const image_read_settings< FormatTag >& settings
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_input_device< Device >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    return detail::reader< Device
                         , FormatTag
                         , detail::read_and_no_convert
                         >( file
                          , settings
                          ).get_info();
}

/// \brief Returns the image info. Image info is file format specific.
/// \param file It's a device. Must satisfy is_input_device metafunction.
/// \param tag  Defines the image format. Must satisfy is_format_tag metafunction. 
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename Device
        , typename FormatTag
        >
inline
image_read_info< FormatTag >
read_image_info( Device&          file
               , const FormatTag&
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_input_device< Device >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    return read_image_info( file
                          , image_read_settings< FormatTag >()
                          );
}


/// \brief Returns the image info. Image info is file format specific.
/// \param file      It's a device. Must satisfy is_adaptable_input_device metafunction.
/// \param settings  Specifies read settings depending on the image format.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename Device
        , typename FormatTag
        >
inline 
image_read_info<FormatTag>
read_image_info( Device&                                 file
               , const image_read_settings< FormatTag >& settings
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_adaptable_input_device< FormatTag
                                                                                 , Device
                                                                                 >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    typedef typename detail::is_adaptable_input_device< FormatTag
                                                      , Device
                                                      >::device_type device_type;

    device_type dev( file );

    return detail::reader< device_type
                         , FormatTag
                         , detail::read_and_no_convert
                         >( dev
                          , settings
                          ).get_info();
}

/// \brief Returns the image info. Image info is file format specific.
/// \param file It's a device. Must satisfy is_adaptable_input_device metafunction.
/// \param tag  Defines the image format. Must satisfy is_format_tag metafunction. 
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename Device
        , typename FormatTag
        >
inline 
image_read_info< FormatTag >
read_image_info( Device&          file
               , const FormatTag&
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_adaptable_input_device< FormatTag
                                                                                 , Device
                                                                                 >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    typedef typename detail::is_adaptable_input_device< FormatTag
                                                      , Device
                                                      >::device_type device_type;

    device_type dev( file );

    return read_image_info( dev
                          , image_read_settings< FormatTag >()
                          );
}

/// \brief Returns the image info. Image info is file format specific.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param settings  Specifies read settings depending on the image format.
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename String
        , typename FormatTag
        >
inline 
image_read_info< FormatTag >
read_image_info( const String&                           file_name
               , const image_read_settings< FormatTag >& settings
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_supported_path_spec< String >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    detail::file_stream_device< FormatTag > reader( detail::convert_to_string( file_name )
                                                  , typename detail::file_stream_device< FormatTag >::read_tag()
                                                  );

    return read_image_info( reader
                          , settings
                          );
}

/// \brief Returns the image info. Image info is file format specific.
/// \param file_name File name. Must satisfy is_supported_path_spec metafunction.
/// \param tag       Defines the image format. Must satisfy is_format_tag metafunction. 
/// \return image_read_info object dependent on the image format.
/// \throw std::ios_base::failure
template< typename String
        , typename FormatTag
        >
inline 
image_read_info< FormatTag >
read_image_info( const String&    file_name
               , const FormatTag&
               , typename enable_if< mpl::and_< is_format_tag< FormatTag >
                                              , detail::is_supported_path_spec< String >
                                              >
                                   >::type* /* ptr */ = 0
               )
{
    detail::file_stream_device< FormatTag > reader( detail::convert_to_string( file_name )
                                                  , typename detail::file_stream_device< FormatTag >::read_tag()
                                                  );

    return read_image_info( reader
                          , image_read_settings< FormatTag >()
                          );
}

} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_IMAGE_READ_INFO_HPP
