/*
    Copyright 2007-2008 Christian Henning, Andreas Pokorny, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_DETAIL_PNG_IO_WRITE_HPP
#define BOOST_GIL_EXTENSION_IO_DETAIL_PNG_IO_WRITE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning, Andreas Pokorny, Lubomir Bourdev \n
///
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

extern "C" {
#include <png.h>
}

#include <boost/gil/extension/io_new/detail/typedefs.hpp>
#include <boost/gil/extension/io_new/detail/base.hpp>
#include <boost/gil/extension/io_new/detail/row_buffer_helper.hpp>

#include "base.hpp"
#include "supported_types.hpp"

namespace boost { namespace gil { namespace detail {

template<typename Device>
class writer< Device
            , png_tag
            > : png_io_base< Device >
{

public:

    writer( Device& io_dev )
    : png_io_base< Device >( io_dev )
    , _png_ptr ( NULL )
    , _info_ptr( NULL )
    {
        // Create and initialize the png_struct with the desired error handler
        // functions.  If you want to use the default stderr and longjump method,
        // you can supply NULL for the last three parameters.  We also check that
        // the library version is compatible with the one used at compile time,
        // in case we are using dynamically linked libraries.  REQUIRED.
        _png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING
                                          , NULL  // user_error_ptr
                                          , NULL  // user_error_fn
                                          , NULL  // user_warning_fn
                                          );

        io_error_if( _png_ptr == NULL
                   , "png_writer: fail to call png_create_write_struct()"
                   );

        // Allocate/initialize the image information data.  REQUIRED 
        _info_ptr = png_create_info_struct( _png_ptr );

        if( _info_ptr == NULL )
        {
            png_destroy_write_struct( &_png_ptr
                                    , NULL
                                    );

            io_error( "png_writer: fail to call png_create_info_struct()" );
        }

        // Set error handling.  REQUIRED if you aren't supplying your own
        // error handling functions in the png_create_write_struct() call.
        if( setjmp( png_jmpbuf( _png_ptr )))
        {
            //free all of the memory associated with the png_ptr and info_ptr
            png_destroy_write_struct( &_png_ptr
                                    , &_info_ptr
                                    );

            io_error( "png_writer: fail to call setjmp()" );
        }

        this->init_io( _png_ptr );
    }

    ~writer()
    {
        png_destroy_write_struct( &_png_ptr
                                , &_info_ptr
                                );
    }

    template< typename View >
    void apply( const View& view )
    {
        apply( view, image_write_info< png_tag >() );
    }

    template <typename View>
    void apply( const View&                        view
              , const image_write_info< png_tag >& info
              )
    {
        typedef png_write_support< typename channel_type    < typename get_pixel_type< View >::type >::type
                                 , typename color_space_type< View >::type
                                 > png_rw_info_t;

        io_error_if( view.width() == 0 && view.height() == 0
                   , "png format cannot handle empty views."
                   );

        // Set the image information here.  Width and height are up to 2^31,
        // bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
        // the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
        // PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
        // or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
        // PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
        // currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
        png_set_IHDR( _png_ptr
                    , _info_ptr
                    , static_cast< png_image_width::type  >( view.width()  )
                    , static_cast< png_image_height::type >( view.height() )
                    , static_cast< png_bitdepth::type     >( png_rw_info_t::_bit_depth )
                    , static_cast< png_color_type::type   >( png_rw_info_t::_color_type )
                    , info._interlace_method
                    , info._compression_method
                    , info._filter_method
                    );

#ifdef BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED
        if( info._valid_cie_colors )
        {
            png_set_cHRM( _png_ptr
                        , _info_ptr
                        , info._white_x
                        , info._white_y
                        , info._red_x
                        , info._red_y
                        , info._green_x
                        , info._green_y
                        , info._blue_x
                        , info._blue_y
                        );
        }

        if( info._valid_file_gamma )
        {
            png_set_gAMA( _png_ptr
                        , _info_ptr
                        , info._gamma
                        );
        }
#else
        if( info._valid_cie_colors )
        {
            png_set_cHRM_fixed( _png_ptr
                              , _info_ptr
                              , info._white_x
                              , info._white_y
                              , info._red_x
                              , info._red_y
                              , info._green_x
                              , info._green_y
                              , info._blue_x
                              , info._blue_y
                              );
        }

        if( info._valid_file_gamma )
        {
            png_set_gAMA_fixed( _png_ptr
                              , _info_ptr
                              , info._file_gamma
                              );
        }
#endif // BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED

        if( info._valid_icc_profile )
        {
            png_set_iCCP( _png_ptr
                        , _info_ptr
                        , const_cast< png_charp >( info._icc_name.c_str() )
                        , info._iccp_compression_type
                        , const_cast< png_charp >( info._profile.c_str() )
                        , info._profile_length
                        );
        }

        if( info._valid_intent )
        {
            png_set_sRGB( _png_ptr
                        , _info_ptr
                        , info._intent
                        );
        }

        if( info._valid_palette )
        {
            png_set_PLTE( _png_ptr
                        , _info_ptr
                        , const_cast< png_colorp >( &info._palette.front() )
                        , info._num_palette
                        );
        }

        if( info._valid_background )
        {
            png_set_bKGD( _png_ptr
                        , _info_ptr
                        , const_cast< png_color_16p >( &info._background )
                        );
        }

        if( info._valid_histogram )
        {
            png_set_hIST( _png_ptr
                        , _info_ptr
                        , const_cast< png_uint_16p >( &info._histogram.front() )
                        );
        }

        if( info._valid_offset )
        {
            png_set_oFFs( _png_ptr
                        , _info_ptr
                        , info._offset_x
                        , info._offset_y
                        , info._off_unit_type
                        );
        }

        if( info._valid_pixel_calibration )
        {
            std::vector< const char* > params( info._num_params );
            for( std::size_t i = 0; i < params.size(); ++i )
            {
                params[i] = info._params[ i ].c_str();
            }

            png_set_pCAL( _png_ptr
                        , _info_ptr
                        , const_cast< png_charp >( info._purpose.c_str() )
                        , info._X0
                        , info._X1
                        , info._cal_type
                        , info._num_params
                        , const_cast< png_charp  >( info._units.c_str() )
                        , const_cast< png_charpp >( &params.front()     )
                        );
        }

        if( info._valid_resolution )
        {
            png_set_pHYs( _png_ptr
                        , _info_ptr
                        , info._res_x
                        , info._res_y
                        , info._phy_unit_type
                        );
        }

        if( info._valid_significant_bits )
        {
            png_set_sBIT( _png_ptr
                        , _info_ptr
                        , const_cast< png_color_8p >( &info._sig_bits )
                        );
        }

#ifdef BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED 
        if( info._valid_scale_factors )
        {
            png_set_sCAL( _png_ptr
                        , _info_ptr
                        , info._scale_unit
                        , info._scale_width
                        , info._scale_height
                        );
        }
#else
#ifdef BOOST_GIL_IO_PNG_FIXED_POINT_SUPPORTED

        if( info._valid_scale_factors )
        {
            png_set_sCAL_s( _png_ptr
                          , _info_ptr
                          , _scale_unit
                          , const_cast< png_charp >( _scale_width.c_str()  )
                          , const_cast< png_charp >( _scale_height.c_str() )
                          );
        }
#endif // BOOST_GIL_IO_PNG_FIXED_POINT_SUPPORTED
#endif // BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED


        if( info._valid_text )
        {
            std::vector< png_text > texts( info._num_text );
            for( std::size_t i = 0; i < texts.size(); ++i )
            {
                png_text pt;
                pt.compression = info._text[i]._compression;
                pt.key         = const_cast< png_charp >( info._text[i]._key.c_str()  );
                pt.text        = const_cast< png_charp >( info._text[i]._text.c_str() );
                pt.text_length = info._text[i]._text.length();

                texts[i] = pt;
            }

            png_set_text( _png_ptr
                        , _info_ptr
                        , &texts.front()
                        , info._num_text
                        );
        }

        if( info._valid_modification_time )
        {
            png_set_tIME( _png_ptr
                        , _info_ptr
                        , const_cast< png_timep >( &info._mod_time )
                        );
        }

        if( info._valid_transparency_factors )
        {
            int sample_max = ( 1 << info._bit_depth );

            /* libpng doesn't reject a tRNS chunk with out-of-range samples */
            if( !(  (  info._color_type == PNG_COLOR_TYPE_GRAY 
                    && (int) info._trans_values[0].gray > sample_max
                    )
                 || (  info._color_type == PNG_COLOR_TYPE_RGB
                    &&(  (int) info._trans_values[0].red   > sample_max 
                      || (int) info._trans_values[0].green > sample_max
                      || (int) info._trans_values[0].blue  > sample_max
                      )
                    )
                 )
              )
            {
                //@todo Fix that once reading transparency values works
/*
                png_set_tRNS( _png_ptr
                            , _info_ptr
                            , trans
                            , num_trans
                            , trans_values
                            );
*/
            }
        }

        png_write_info( _png_ptr
                      ,_info_ptr
                      );

        write_view( view
                  , typename is_bit_aligned< View >::type()
                  );
    }

private:

    template<typename View>
    void write_view( const View& view
                   ,  mpl::false_       // is bit aligned
                   )
    {
        typedef typename get_pixel_type< View >::type pixel_t;

        typedef png_write_support< typename channel_type    < pixel_t >::type
                                 , typename color_space_type< pixel_t >::type
                                 > png_rw_info;

        if( little_endian() )
        {
            if( png_rw_info::_bit_depth == 16 )
            {
                png_set_swap( _png_ptr );
            }

            if( png_rw_info::_bit_depth < 8 )
            {
                png_set_packswap( _png_ptr );
            }
        }

        row_buffer_helper_view< View > row_buffer( view.width()
                                                 , false
                                                 );

        for( int y = 0; y != view.height(); ++ y)
        {
            std::copy( view.row_begin( y )
                     , view.row_end  ( y )
                     , row_buffer.begin()
                     );

            png_write_row( _png_ptr
                         , reinterpret_cast< png_bytep >( row_buffer.data() )
                         );
        }

        png_write_end( _png_ptr
                     , _info_ptr
                     );
    }

    template<typename View>
    void write_view( const View& view
                   , mpl::true_         // is bit aligned
                   )
    {
        typedef png_write_support< typename kth_semantic_element_type< typename View::value_type
                                                                     , 0
                                                                     >::type
                                 , typename color_space_type<View>::type
                                 > png_rw_info;

        if (little_endian() )
        {
            if( png_rw_info::_bit_depth == 16 )
            {
                png_set_swap( _png_ptr );
            }

            if( png_rw_info::_bit_depth < 8 )
            {
                png_set_packswap( _png_ptr );
            }
        }

        row_buffer_helper_view< View > row_buffer( view.width()
                                                 , false
                                                 );

        for( int y = 0; y != view.height(); ++y )
        {
            std::copy( view.row_begin( y )
                     , view.row_end  ( y )
                     , row_buffer.begin()
                     );

            png_write_row( _png_ptr
                         , reinterpret_cast< png_bytep >( row_buffer.data() )
                         );
        }

        png_free_data( _png_ptr
                     , _info_ptr
                     , PNG_FREE_UNKN
                     , -1
                     );

        png_write_end( _png_ptr
                     , _info_ptr
                     );
    }

    void init_io( png_structp png_ptr )
    {
        png_set_write_fn( png_ptr
                        , static_cast< void* >        ( &this->_io_dev                   )
                        , static_cast< png_rw_ptr >   ( &png_io_base<Device>::write_data )
                        , static_cast< png_flush_ptr >( &png_io_base<Device>::flush      )
                        );
    }

    png_structp _png_ptr;
    png_infop _info_ptr;
};

struct png_write_is_supported
{
    template< typename View >
    struct apply 
        : public is_write_supported< typename get_pixel_type< View >::type
                                   , png_tag
                                   >
    {};
};

template< typename Device >
class dynamic_image_writer< Device
                          , png_tag
                          >
    : public writer< Device
                   , png_tag
                   >
{
    typedef writer< Device
                  , png_tag
                  > parent_t;

public:

    dynamic_image_writer( Device& file )
    : parent_t( file )
    {}

    template< typename Views >
    void apply( const any_image_view< Views >& views )
    {
        dynamic_io_fnobj< png_write_is_supported
                        , parent_t
                        > op( this );

        apply_operation( views, op );
    }
};

} // namespace detail
} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_DETAIL_PNG_IO_WRITE_HPP
