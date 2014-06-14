/*
    Copyright 2008 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_BMP_IO_WRITE_HPP
#define BOOST_GIL_EXTENSION_IO_BMP_IO_WRITE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <vector>

#include <boost/gil/extension/io_new/detail/base.hpp>
#include <boost/gil/extension/io_new/detail/io_device.hpp>

#include <boost/gil/extension/io_new/bmp_tags.hpp>

namespace boost { namespace gil { namespace detail {

template < int N > struct get_bgr_cs {};
template <> struct get_bgr_cs< 1 > { typedef gray8_view_t type; };
template <> struct get_bgr_cs< 3 > { typedef bgr8_view_t type; };
template <> struct get_bgr_cs< 4 > { typedef bgra8_view_t type; };

template< typename Device >
class writer< Device
            , bmp_tag
            >
{
public:

    writer( Device& file )
    : _out( file )
    {
    }

    ~writer()
    {
    }

    template<typename View>
    void apply( const View& view )
    {
        write( view );
    }

    template<typename View>
    void apply( const View&                           view
              , const image_write_info< bmp_tag >& /* info */
              )
    {
        // Add code here, once image_write_info< bmp_tag > isn't empty anymore.

        write( view );
    }

private:

    template< typename View >
    void write( const View& view )
    {
        typedef typename channel_type<
                    typename get_pixel_type< View >::type >::type channel_t;

        typedef typename color_space_type< View >::type color_space_t;

        // check if supported
/*
        /// todo
        if( bmp_read_write_support_private<channel_t, color_space_t>::channel != 8)
        {
            io_error("Input view type is incompatible with the image type");
        }
*/

        // compute the file size
        int bpp = num_channels< View >::value * 8;
        int entries = 0;

/*
        /// @todo: Not supported for now. bit_aligned_images refer to indexed images
        ///        in this context.
        if( bpp <= 8 )
        {
            entries = 1 << bpp;
        }
*/

        std::size_t spn = ( view.width() * num_channels< View >::value + 3 ) & ~3;
        std::size_t ofs = bmp_header_size::_size 
                        + bmp_header_size::_win32_info_size 
                        + entries * 4;

        std::size_t siz = ofs + spn * view.height();

        // write the BMP file header
        _out.write_int16( bmp_signature );
        _out.write_int32( (uint32_t) siz );
        _out.write_int16( 0 );
        _out.write_int16( 0 );
        _out.write_int32( (uint32_t) ofs );

        // writes Windows information header
        _out.write_int32( bmp_header_size::_win32_info_size );
        _out.write_int32( static_cast< uint32_t >( view.width()  ));
        _out.write_int32( static_cast< uint32_t >( view.height() ));
        _out.write_int16( 1 );
        _out.write_int16( static_cast< uint16_t >( bpp ));
        _out.write_int32( bmp_compression::_rgb );
        _out.write_int32( 0 );
        _out.write_int32( 0 );
        _out.write_int32( 0 );
        _out.write_int32( entries );
        _out.write_int32( 0 );

        write_image< View
                   , typename get_bgr_cs< num_channels< View >::value >::type
                   >( view, spn );
    }


    template< typename View
            , typename BMP_View
            >
    void write_image( const View&       view
                    , const std::size_t spn
                    )
    {
        byte_vector_t buffer( spn );
        std::fill( buffer.begin(), buffer.end(), 0 );


        BMP_View row = interleaved_view( view.width()
                                       , 1
                                       , (typename BMP_View::value_type*) &buffer.front()
                                       , spn
                                       );

        for( typename View::y_coord_t y = view.height() - 1; y > -1; --y  )
        {
            copy_pixels( subimage_view( view
                                      , 0
                                      , (int) y
                                      , (int) view.width()
                                      , 1
                                      )
                       , row
                       );

            _out.write( &buffer.front(), spn );
        }

    }

private:

    Device& _out;
};


struct bmp_write_is_supported
{
    template< typename View >
    struct apply
        : public is_write_supported< typename get_pixel_type< View >::type
                                   , bmp_tag
                                   >
    {};
};

template< typename Device >
class dynamic_image_writer< Device
                          , bmp_tag
                          >
    : public writer< Device
                   , bmp_tag
                   >
{
    typedef writer< Device
                  , bmp_tag
                  > parent_t;

public:

    dynamic_image_writer( Device& file )
    : parent_t( file )
    {}

    template< typename Views >
    void apply( const any_image_view< Views >& views )
    {
        dynamic_io_fnobj< bmp_write_is_supported
                        , parent_t
                        > op( this );

        apply_operation( views, op );
    }
};

} // detail
} // gil
} // boost

#endif // BOOST_GIL_EXTENSION_IO_BMP_IO_WRITE_HPP
