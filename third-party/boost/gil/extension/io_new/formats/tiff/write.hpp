/*
    Copyright 2007-2008 Christian Henning, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_TIFF_IO_WRITE_HPP
#define BOOST_GIL_EXTENSION_IO_TIFF_IO_WRITE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning, Lubomir Bourdev \n
///
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

extern "C" {
#include "tiff.h"
#include "tiffio.h"
}

#include <algorithm>
#include <string>
#include <vector>
#include <boost/static_assert.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/gil/extension/io_new/tiff_tags.hpp>

#include <boost/gil/extension/io_new/detail/base.hpp>
#include <boost/gil/extension/io_new/detail/io_device.hpp>

#include "device.hpp"

namespace boost { namespace gil { namespace detail {

template <typename PixelReference>
struct my_interleaved_pixel_iterator_type_from_pixel_reference
{
private:
	typedef typename remove_reference< PixelReference >::type::value_type pixel_t;
public:
	typedef typename iterator_type_from_pixel< pixel_t
	                                         , false
	                                         , false
	                                         , true
	                                         >::type type;
};


template< typename Channel
        , typename Layout
        , bool Mutable
        >
struct my_interleaved_pixel_iterator_type_from_pixel_reference< const bit_aligned_pixel_reference< byte_t
                                                                                                 , Channel
                                                                                                 , Layout
                                                                                                 , Mutable
                                                                                                 >
                                                              >
	: public iterator_type_from_pixel< const bit_aligned_pixel_reference< uint8_t
	                                                                    , Channel
	                                                                    , Layout
	                                                                    , Mutable
	                                                                    >
	                                 ,false
	                                 ,false
	                                 ,true
	                                 > {};





template < typename Channel > struct sample_format : public mpl::int_<SAMPLEFORMAT_UINT> {};
template<> struct sample_format<bits8>   : public mpl::int_<SAMPLEFORMAT_UINT> {};
template<> struct sample_format<bits16>  : public mpl::int_<SAMPLEFORMAT_UINT> {};
template<> struct sample_format<bits32>  : public mpl::int_<SAMPLEFORMAT_UINT> {};
template<> struct sample_format<bits32f> : public mpl::int_<SAMPLEFORMAT_IEEEFP> {};
template<> struct sample_format<double>  : public mpl::int_<SAMPLEFORMAT_IEEEFP> {};
template<> struct sample_format<bits8s>  : public mpl::int_<SAMPLEFORMAT_INT> {};
template<> struct sample_format<bits16s> : public mpl::int_<SAMPLEFORMAT_INT> {};
template<> struct sample_format<bits32s> : public mpl::int_<SAMPLEFORMAT_INT> {};

template <typename Channel> struct photometric_interpretation {};
template<> struct photometric_interpretation< gray_t > : public mpl::int_< PHOTOMETRIC_MINISBLACK > {};
template<> struct photometric_interpretation< rgb_t  > : public mpl::int_< PHOTOMETRIC_RGB        > {};
template<> struct photometric_interpretation< rgba_t > : public mpl::int_< PHOTOMETRIC_RGB        > {};
template<> struct photometric_interpretation< cmyk_t > : public mpl::int_< PHOTOMETRIC_SEPARATED  > {};

template < typename Device, typename Log >
class writer< Device
            , tiff_tag
            , Log
            >
{
public:

    typedef image_write_info<tiff_tag, Log> info_t;

    writer( Device& dev )
    : _io_dev( dev ) {}

    template<typename View>
    void apply( const View& view )
    {
        typedef typename color_space_type<View>::type color_space_t;

        info_t info;

        // write photometric interpretion - Warning: This value is rather subjective.
        // The user should better set this value itself. There is no way to decide if
        // a image is PHOTOMETRIC_MINISWHITE or PHOTOMETRIC_MINISBLACK. This writer
        // will assume PHOTOMETRIC_MINISBLACK for gray_t images and PHOTOMETRIC_RGB
        // for rgb_t images.
        info._photometric_interpretation = photometric_interpretation< color_space_t >::value;

        write_view( view
                  , info );
    }

    template<typename View>
    void apply( const View&   view
              , const info_t& info )
    {
        write_view( view
                  , info );
    }

private:

    template< typename View >
    void write_view( const View&   src_view
                   , const info_t& info
                   )
    {
        typedef typename View::value_type pixel_t;

        // get the type of the first channel (heterogeneous pixels might be broken for now!)
        typedef typename channel_traits< typename element_type< pixel_t >::type >::value_type channel_t;

        // write dimensions
        tiff_image_width::type  width  = (tiff_image_width::type)  src_view.width();
        tiff_image_height::type height = (tiff_image_height::type) src_view.height();

        _io_dev.template set_property< tiff_image_width  >( width  );
        _io_dev.template set_property< tiff_image_height >( height );

        // write planar configuration
        if( is_bit_aligned< View >::value == false )
        {
            _io_dev.template set_property<tiff_planar_configuration>( info._planar_configuration );
        }

        // write samples per pixel
        tiff_samples_per_pixel::type samples_per_pixel = num_channels< pixel_t >::value;
        _io_dev.template set_property<tiff_samples_per_pixel>( samples_per_pixel );

        // write bits per sample
        // @todo: Settings this value usually requires to write for each sample the bit
        // value seperately in case they are different, like rgb556.
        tiff_bits_per_sample::type bits_per_sample = unsigned_integral_num_bits<channel_t>::value;
        _io_dev.template set_property<tiff_bits_per_sample>( bits_per_sample );

        // write sample format
        tiff_sample_format::type sampl_format = sample_format< channel_t >::type::value;
        _io_dev.template set_property<tiff_sample_format>( sampl_format );

        // write photometric format
        _io_dev.template set_property<tiff_photometric_interpretation>( info._photometric_interpretation );

        // write compression
        _io_dev.template set_property<tiff_compression>( info._compression );

        // write orientation
        _io_dev.template set_property<tiff_orientation>( info._orientation );

        // write rows per strip
        _io_dev.template set_property<tiff_rows_per_strip>( _io_dev.get_default_strip_size() );

        if(!info._is_tiled)
        {
            write_data( src_view
                      , info
                      , (src_view.width() * samples_per_pixel * bits_per_sample + 7) / 8
                      , typename is_bit_aligned< pixel_t >::type()
                      );
        }
        else
        {
            tiff_tile_width::type  tw = info._tile_width;
            tiff_tile_length::type th = info._tile_length;

            if(!_io_dev.check_tile_size( tw, th ))
            {
                io_error( "Tile sizes need to be multiples of 16." );
            }

            // tile related tags
            _io_dev.template set_property<tiff_tile_width> ( tw );
            _io_dev.template set_property<tiff_tile_length>( th );

            write_tiled_data( src_view
                            , tw
                            , th
                            , typename is_bit_aligned< pixel_t >::type()
                            );
        }
    }

    template< typename View >
    void write_data( const View&   src_view
                   , const info_t& /* info */
                   , std::size_t   row_size_in_bytes
                   , const mpl::true_&    // bit_aligned
                   )
    {
        byte_vector_t row( row_size_in_bytes );

        typedef typename View::x_iterator x_it_t;
        x_it_t row_it = x_it_t( &(*row.begin()));

        for( typename View::y_coord_t y = 0; y < src_view.height(); ++y )
        {
            std::copy( src_view.row_begin( y )
                     , src_view.row_end( y )
                     , row_it
                     );

            _io_dev.write_scaline( row
                                 , (uint32) y
                                 , 0
                                 );

            // @todo: do optional bit swapping here if you need to...
        }
    }

    template< typename View >
    void write_tiled_data( const View&            src_view
                         , tiff_tile_width::type  tw
                         , tiff_tile_length::type th
                         , const mpl::true_&    // bit_aligned
                         )
    {
        byte_vector_t row( _io_dev.get_tile_size() );

        typedef typename View::x_iterator x_it_t;
        x_it_t row_it = x_it_t( &(*row.begin()));

        internal_write_tiled_data(src_view, tw, th, row, row_it);
    }

    template< typename View >
    void write_data( const View&   src_view
                   , const info_t& info
                   , std::size_t   row_size_in_bytes
                   , const mpl::false_&    // bit_aligned
                   )
    {
        byte_vector_t row( row_size_in_bytes );

        typedef typename my_interleaved_pixel_iterator_type_from_pixel_reference< typename View::reference
                                                                                >::type x_iterator;

        x_iterator row_it = x_iterator( &(*row.begin()));

        for( typename View::y_coord_t y = 0; y < src_view.height(); ++y )
        {
            std::copy( src_view.row_begin( y )
                     , src_view.row_end( y )
                     , row_it
                     );

            _io_dev.write_scaline( row
                                 , (uint32) y
                                 , 0
                                 );

            // @todo: do optional bit swapping here if you need to...
        }
    }

    template< typename View >
    void write_tiled_data( const View&            src_view
                         , tiff_tile_width::type  tw
                         , tiff_tile_length::type th
                         , const mpl::false_&    // bit_aligned
                         )
    {
        byte_vector_t row( _io_dev.get_tile_size() );

        typedef typename my_interleaved_pixel_iterator_type_from_pixel_reference< typename View::reference
                                                                                >::type x_iterator;
        x_iterator row_it = x_iterator( &(*row.begin()));

        internal_write_tiled_data(src_view, tw, th, row, row_it);
    }

    template< typename View,
              typename IteratorType >
    void internal_write_tiled_data( const View&            src_view
                                  , tiff_tile_width::type  tw
                                  , tiff_tile_length::type th
                                  , byte_vector_t&         row
                                  , IteratorType           it
                                  )
    {
        std::ptrdiff_t i = 0, j = 0;
        View tile_subimage_view;
        while( i < src_view.height() )
        {
            while( j < src_view.width() )
            {
                if( j + tw < src_view.width() && i + th < src_view.height() )
                {
                    // a tile is fully included in the image: just copy values
                    tile_subimage_view = subimage_view( src_view
                                                      , static_cast< int >( j  )
                                                      , static_cast< int >( i  )
                                                      , static_cast< int >( tw )
                                                      , static_cast< int >( th )
                                                      );

                    std::copy( tile_subimage_view.begin()
                             , tile_subimage_view.end()
                             , it
                             );
                }
                else
                {
                    std::ptrdiff_t width  = src_view.width();
                    std::ptrdiff_t height = src_view.height();

                    std::ptrdiff_t current_tile_width  = ( j + tw < width ) ? tw : width  - j;
                    std::ptrdiff_t current_tile_length = ( i + th < height) ? th : height - i;

                    tile_subimage_view = subimage_view( src_view
                                                      , static_cast< int >( j )
                                                      , static_cast< int >( i )
                                                      , static_cast< int >( current_tile_width )
                                                      , static_cast< int >( current_tile_length )
                                                      );

                    for( typename View::y_coord_t y = 0; y < tile_subimage_view.height(); ++y )
                    {
                        std::copy( tile_subimage_view.row_begin( y )
                                 , tile_subimage_view.row_end( y )
                                 , it
                                 );
                        std::advance(it, tw);
                    }

                    it = IteratorType( &(*row.begin()));
                }

                _io_dev.write_tile( row
                                  , static_cast< uint32 >( j )
                                  , static_cast< uint32 >( i )
                                  , 0
                                  , 0
                                  );
                j += tw;
            }
            j = 0;
            i += th;
        }
        // @todo: do optional bit swapping here if you need to...
    }

    Device& _io_dev;
};

struct tiff_write_is_supported
{
    template< typename View >
    struct apply 
        : public is_write_supported< typename get_pixel_type< View >::type
                                   , tiff_tag
                                   >
    {};
};

template< typename Device >
class dynamic_image_writer< Device
                          , tiff_tag
                          >
    : public writer< Device
                   , tiff_tag
                   >
{
    typedef writer< Device
                  , tiff_tag
                  > parent_t;

public:

    dynamic_image_writer( Device& file )
    : parent_t( file )
    {}

    template< typename Views >
    void apply( const any_image_view< Views >& views )
    {
        dynamic_io_fnobj< tiff_write_is_supported
                        , parent_t
                        > op( this );

        apply_operation( views, op );
    }
};

} // namespace detail
} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_TIFF_IO_WRITE_HPP
