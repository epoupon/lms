/*
    Copyright 2007-2008 Christian Henning, Andreas Pokorny, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_WRITE_VIEW_HPP
#define BOOST_GIL_EXTENSION_IO_WRITE_VIEW_HPP

#include <boost/type_traits/is_base_and_derived.hpp>
#include <boost/type_traits/is_base_and_derived.hpp>
#include <boost/mpl/and.hpp>
#include <boost/utility/enable_if.hpp>

#include "base.hpp"
#include "io_device.hpp"
#include "path_spec.hpp"
#include "conversion_policies.hpp"

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning, Andreas Pokorny, Lubomir Bourdev \n
///
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

namespace boost{ namespace gil {

/// \ingroup IO
template< typename Device
        , typename View
        , typename FormatTag
        >
inline
void write_view( Device&          device
               , const View&      view
               , const FormatTag&
               , typename enable_if< typename mpl::and_< typename detail::is_output_device< Device >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       , typename is_write_supported< typename get_pixel_type< View >::type
                                                                                    , FormatTag
                                                                                    >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::writer< Device
                  , FormatTag
                  > writer( device );

    writer.apply( view );
}

template< typename Device
        , typename View
        , typename FormatTag
        >
inline
void write_view( Device&          device
               , const View&      view
               , const FormatTag& tag
               , typename enable_if< typename mpl::and_< typename detail::is_adaptable_output_device< FormatTag
                                                                                                    , Device
                                                                                                    >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       , typename is_write_supported< typename get_pixel_type< View >::type
                                                                                    , FormatTag
                                                                                    >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
        )
{
    typedef typename detail::is_adaptable_output_device< FormatTag
                                                       , Device
                                                       >::device_type dev_t;
    dev_t dev( device );

    write_view( dev
              , view
              , tag
              );
}

template< typename String
        , typename View
        , typename FormatTag
        >
inline
void write_view( const String&    file_name
               , const View&      view
               , const FormatTag& tag
               , typename enable_if< typename mpl::and_< typename detail::is_supported_path_spec< String >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       , typename is_write_supported< typename get_pixel_type< View >::type
                                                                                    , FormatTag
                                                                                    >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::file_stream_device<FormatTag> device( detail::convert_to_string( file_name )
                                                , typename detail::file_stream_device<FormatTag>::write_tag()
                                                );

    write_view( device
               , view
               , tag
               );
}

/// \ingroup IO
template< typename Device
        , typename View
        , typename FormatTag
        , typename Log
        >
inline
void write_view( Device&                            device
               , const View&                        view
               , const image_write_info<FormatTag, Log>& info
               , typename enable_if< typename mpl::and_< typename detail::is_output_device< Device >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       , typename is_write_supported< typename get_pixel_type< View >::type
                                                                                    , FormatTag
                                                                                    >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::writer< Device
                  , FormatTag
                  , Log
                  > writer( device );

    writer.apply( view
                , info );
}

template< typename Device
        , typename View
        , typename FormatTag
        , typename Log
        >
inline
void write_view( Device&                                   device
               , const View&                               view
               , const image_write_info< FormatTag, Log >& info
               , typename enable_if< typename mpl::and_< typename detail::is_adaptable_output_device< FormatTag
                                                                                                    , Device
                                                                                                    >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       , typename is_write_supported< typename get_pixel_type< View >::type
                                                                                    , FormatTag
                                                                                    >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    typename detail::is_adaptable_output_device< FormatTag
                                               , Device
                                               >::device_type dev( device );

    write_view( dev
              , view
              , info
              );
}

template< typename String
        , typename View
        , typename FormatTag
        , typename Log
        >
inline
void write_view( const String&                        file_name
               , const View&                          view
               , const image_write_info< FormatTag, Log >& info
               , typename enable_if< typename mpl::and_< typename detail::is_supported_path_spec< String >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       , typename is_write_supported< typename get_pixel_type< View >::type
                                                                                    , FormatTag
                                                                                    >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::file_stream_device< FormatTag > device( detail::convert_to_string( file_name )
                                                  , typename detail::file_stream_device< FormatTag >::write_tag()
                                                  );

    write_view( device
              , view
              , info
              );
}


////////////////////////////////////// dynamic_image

// without image_write_info
template< typename Device
        , typename Views
        , typename FormatTag
        >
inline
void write_view( Device&                        device
               , const any_image_view< Views >& view
               , const FormatTag&
               , typename enable_if< typename mpl::and_< typename detail::is_output_device< Device >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::dynamic_image_writer< Device
                                , FormatTag
                                > dyn_writer( device );

    dyn_writer.apply( view );
}

template< typename Device
        , typename Views
        , typename FormatTag
        >
inline
void write_view( Device&                        device
               , const any_image_view< Views >& views
               , const FormatTag&               tag
               , typename enable_if< typename mpl::and_< typename detail::is_adaptable_output_device< FormatTag
                                                                                                    , Device
                                                                                                    >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
        )
{
    typedef typename detail::is_adaptable_output_device< FormatTag
                                                       , Device
                                                       >::device_type dev_t;
    dev_t dev( device );

    write_view( dev
              , views
              , tag
              );
}

template< typename String
        , typename Views
        , typename FormatTag
        >
inline
void write_view( const String&                  file_name
               , const any_image_view< Views >& views
               , const FormatTag&               tag
               , typename enable_if< typename mpl::and_< typename detail::is_supported_path_spec< String >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::file_stream_device<FormatTag> device( detail::convert_to_string( file_name )
                                                , typename detail::file_stream_device<FormatTag>::write_tag()
                                                );

    write_view( device
               , views
               , tag
               );
}

// with image_write_info
/// \ingroup IO
template< typename Device
        , typename Views
        , typename FormatTag
        , typename Log
        >
inline
void write_view( Device&                           device
               , const any_image_view< Views >&    views
               , const image_write_info< FormatTag
                                       , Log
                                       >&           info
               , typename enable_if< typename mpl::and_< typename detail::is_output_device< Device >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::dynamic_image_writer< Device
                  , FormatTag
                  , Log
                  > dyn_writer( device );

    dyn_writer.apply( views
                    , info
                    );
}

template< typename Device
        , typename Views
        , typename FormatTag
        , typename Log
        >
inline
void write_view( Device&                           device
               , const any_image_view< Views >&    views
               , const image_write_info< FormatTag
                                       , Log
                                       >&          info
               , typename enable_if< typename mpl::and_< typename detail::is_adaptable_output_device< FormatTag
                                                                                                    , Device
                                                                                                    >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    typename detail::is_adaptable_output_device< FormatTag
                                               , Device
                                               >::device_type dev( device );

    write_view( dev
              , views
              , info
              );
}

template< typename String
        , typename Views
        , typename FormatTag
        , typename Log
        >
inline
void write_view( const String&                      file_name
               , const any_image_view< Views >&     views
               , const image_write_info< FormatTag
                                       , Log
                                       >&           info
               , typename enable_if< typename mpl::and_< typename detail::is_supported_path_spec< String >::type
                                                       , typename is_format_tag< FormatTag >::type
                                                       >::type
                                   >::type* /* ptr */ = 0
               )
{
    detail::file_stream_device< FormatTag > device( detail::convert_to_string( file_name )
                                                  , typename detail::file_stream_device< FormatTag >::write_tag()
                                                  );

    write_view( device
              , views
              , info
              );
}

} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_WRITE_VIEW_HPP
