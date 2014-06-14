/*
    Copyright 2007-2008 Christian Henning, Andreas Pokorny, Lubomir Bourdev
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_JPEG_IO_WRITE_HPP
#define BOOST_GIL_EXTENSION_IO_JPEG_IO_WRITE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning, Andreas Pokorny, Lubomir Bourdev \n
///
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <vector>

#include <boost/gil/extension/io_new/detail/base.hpp>
#include <boost/gil/extension/io_new/detail/io_device.hpp>

#include <boost/gil/extension/io_new/jpeg_tags.hpp>

#include "base.hpp"
#include "supported_types.hpp"

namespace boost { namespace gil { namespace detail {

template< typename Device >
class writer< Device
            , jpeg_tag
            > : public jpeg_io_base
{
public:

    writer( Device& file )
    : out( file )
    {
        _cinfo.err         = jpeg_std_error( &_jerr );
        _cinfo.client_data = this;

        // Error exit handler: does not return to caller.
        _jerr.error_exit = &writer< Device, jpeg_tag >::error_exit;

        // Fire exception in case of error.
        if( setjmp( _mark )) { raise_error(); }

        _dest._jdest.free_in_buffer      = sizeof( buffer );
        _dest._jdest.next_output_byte    = buffer;
        _dest._jdest.init_destination    = reinterpret_cast< void(*)   ( j_compress_ptr ) >( &writer< Device, jpeg_tag >::init_device  );
        _dest._jdest.empty_output_buffer = reinterpret_cast< boolean(*)( j_compress_ptr ) >( &writer< Device, jpeg_tag >::empty_buffer );
        _dest._jdest.term_destination    = reinterpret_cast< void(*)   ( j_compress_ptr ) >( &writer< Device, jpeg_tag >::close_device );
        _dest._this = this;

        jpeg_create_compress( &_cinfo  );
        _cinfo.dest = &_dest._jdest;

    }

    ~writer()
    {
        jpeg_finish_compress ( &_cinfo );
        jpeg_destroy_compress( &_cinfo );
    }

    template<typename View>
    void apply( const View& view )
    {
        write_rows( view
                  , jpeg_quality::default_value
                  , jpeg_dct_method::default_value
                  );
    }

    template< typename View >
    void apply( const View&                         view
              , const image_write_info< jpeg_tag >& info )
    {
        write_rows( view
                  , info._quality
                  , info._dct_method
                  );
    }

private:

    template<typename View>
    void write_rows( const View&                 view
                   , const jpeg_quality::type    quality
                   , const jpeg_dct_method::type dct_method
                   )
    {
        std::vector< typename View::value_type > row_buffer( view.width() );

        // In case of an error we'll jump back to here and fire an exception.
        // @todo Is the buffer above cleaned up when the exception is thrown?
        //       The strategy right now is to allocate necessary memory before
        //       the setjmp.
        if( setjmp( _mark )) { raise_error(); }

        typedef typename channel_type< typename View::value_type >::type channel_t;

        _cinfo.image_width      = JDIMENSION( view.width()  );
        _cinfo.image_height     = JDIMENSION( view.height() );
        _cinfo.input_components = num_channels<View>::value;
        _cinfo.in_color_space   = detail::jpeg_write_support< channel_t
                                                            , typename color_space_type< View >::type
                                                            >::_color_space;

        jpeg_set_defaults( &_cinfo );
        jpeg_set_quality ( &_cinfo
                         , quality
                         , TRUE
                         );

        // Needs to be done after jpeg_set_defaults() since it's overridding this value back to slow.
        _cinfo.dct_method = dct_method;
 
        jpeg_start_compress( &_cinfo
                           , TRUE
                           );

        JSAMPLE* row_addr = reinterpret_cast< JSAMPLE* >( &row_buffer[0] );

        for( int y =0; y != view.height(); ++ y )
        {
            std::copy( view.row_begin( y )
                     , view.row_end  ( y )
                     , row_buffer.begin()
                     );

            jpeg_write_scanlines( &this->_cinfo
                                , &row_addr
                                , 1
                                );
        }
    }

    struct gil_jpeg_destination_mgr
    {
        jpeg_destination_mgr _jdest;
        writer<Device,jpeg_tag> * _this;
    };

    static void init_device( jpeg_compress_struct* cinfo )
    {
        gil_jpeg_destination_mgr* dest = reinterpret_cast< gil_jpeg_destination_mgr* >( cinfo->dest );

        dest->_jdest.free_in_buffer   = sizeof( dest->_this->buffer );
        dest->_jdest.next_output_byte = dest->_this->buffer;
    }

    static boolean empty_buffer( jpeg_compress_struct* cinfo )
    {
        gil_jpeg_destination_mgr* dest = reinterpret_cast< gil_jpeg_destination_mgr* >( cinfo->dest );

        dest->_this->out.write( dest->_this->buffer
                              , buffer_size
                              );

        writer<Device,jpeg_tag>::init_device( cinfo );
        return 1;
    }

    static void close_device( jpeg_compress_struct* cinfo )
    {
        writer<Device,jpeg_tag>::empty_buffer( cinfo );

        gil_jpeg_destination_mgr* dest = reinterpret_cast< gil_jpeg_destination_mgr* >( cinfo->dest );

        dest->_this->out.flush();
    }

    void raise_error()
    {
        io_error( "Cannot write jpeg file." );
    }

    static void error_exit( j_common_ptr cinfo )
    {
        writer< Device, jpeg_tag >* mgr = reinterpret_cast< writer< Device, jpeg_tag >* >( cinfo->client_data );

        longjmp( mgr->_mark, 1 );
    }

private:

    Device &out;

    jpeg_compress_struct _cinfo;

    gil_jpeg_destination_mgr _dest;

    static const unsigned int buffer_size = 1024;
    JOCTET buffer[buffer_size];
};

struct jpeg_write_is_supported
{
    template< typename View >
    struct apply
        : public is_write_supported< typename get_pixel_type< View >::type
                                   , jpeg_tag
                                   >
    {};
};

// unary application
template <typename Types, typename Tag, typename Bits, typename Op>
typename Op::result_type GIL_FORCEINLINE apply_operation_basec( const Bits& bits
                                                              , std::size_t index
                                                              , const image_write_info< Tag >& info
                                                              , Op op
                                                              )
{
    return detail::apply_operation_fwd_fn<mpl::size<Types>::value>().template applyc<Types>( bits
                                                                                           , index
                                                                                           , op
                                                                                           );
}

// unary application
template< typename Types
        , typename Info
        , typename Bits
        , typename Op
        >
typename Op::result_type GIL_FORCEINLINE apply_operation_base( Bits&       bits
                                                             , std::size_t index
                                                             , const Info& info
                                                             , Op op
                                                             )
{
    return detail::apply_operation_fwd_fn< mpl::size< Types >::value>().template apply< Types
                                                                                      >( bits
                                                                                       , index
                                                                                       , info
                                                                                       , op
                                                                                       );
}



/// \ingroup Variant
/// \brief Invokes a generic constant operation (represented as a binary function object) on two variants
template< typename Types1
        , typename Info
        , typename BinaryOp
        >
GIL_FORCEINLINE
typename BinaryOp::result_type apply_operation( const variant< Types1 >& arg1
                                              , const Info&              info
                                              , BinaryOp op
                                              )
{
    typename variant< Types1 >::base_t bits = arg1.bits();

    return apply_operation_base< Types1
                               , image_write_info< jpeg_tag >
                               >( bits
                                , arg1.index()
                                , info
                                , op
                                );
}

template< typename Device >
class dynamic_image_writer< Device
                          , jpeg_tag
                          >
    : public writer< Device
                   , jpeg_tag
                   >
{
    typedef writer< Device
                  , jpeg_tag
                  > parent_t;

public:

    dynamic_image_writer( Device& file )
    : parent_t( file )
    {}

    template< typename Views >
    void apply( const any_image_view< Views >& views )
    {
        dynamic_io_fnobj< jpeg_write_is_supported
                        , parent_t
                        > op( this );

        apply_operation( views, op );
    }

    template< typename Views >
    void apply( const any_image_view  < Views    >& views
              , const image_write_info< jpeg_tag >& info
              )
    {
        dynamic_io_fnobj< jpeg_write_is_supported
                        , parent_t
                        > op( this );

        apply_operation( views, info, op );
    }
};

} // detail
} // gil
} // boost

#endif // BOOST_GIL_EXTENSION_IO_JPEG_IO_WRITE_HPP
