/*
    Copyright 2008 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_PNM_IO_WRITE_HPP
#define BOOST_GIL_EXTENSION_IO_PNM_IO_WRITE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Christian Henning \n
///
/// \date 2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <boost/lexical_cast.hpp>

#include <boost/gil/extension/io_new/pnm_tags.hpp>

#include <boost/gil/extension/io_new/detail/base.hpp>
#include <boost/gil/extension/io_new/detail/io_device.hpp>

namespace boost { namespace gil { namespace detail {

template< typename Device >
class writer< Device
            , pnm_tag
            >
{
    typedef image_write_info< pnm_tag > info_t;

public:

    writer( Device & file )
        : _out( file )
    {
    }

    ~writer()
    {
    }

    template<typename View>
    void apply( const View& view )
    {
        info_t info;

        apply( view
             , info );
    }

    template<typename View>
    void apply( const View&   view
              , const info_t& /* info */
              )
    {
        typedef typename get_pixel_type< View >::type pixel_t;

        std::size_t width  = view.width();
        std::size_t height = view.height();

        std::size_t chn    = num_channels< View >::value;
        std::size_t pitch  = chn * width;

        unsigned int type;
        if( num_channels< View >::value == 1 )
        {
            if( is_bit_aligned< pixel_t >::value )
            {
                type = pnm_image_type::mono_bin_t::value;
            }
            else
            {
                type = pnm_image_type::gray_bin_t::value;
            }
        }
        else
        {
            type = pnm_image_type::color_bin_t::value;
        }

        // write header

        // Add a white space at each string so read_int() can decide when a numbers ends.

        std::string str( "P" );
        str += lexical_cast< std::string >( type ) + std::string( " " );
        _out.print_line( str );

        str.clear();
        str += lexical_cast< std::string >( width ) + std::string( " " );
        _out.print_line( str );

        str.clear();
        str += lexical_cast< std::string >( height ) + std::string( " " );
        _out.print_line( str );

        if( type != pnm_image_type::mono_bin_t::value )
        {
            _out.print_line( "255 ");
        }

        // write data
        write_data( view
                  , pitch
                  , typename is_bit_aligned< pixel_t >::type()
                  );
    }

    template< typename View >
    void write_data( const View&   src
                   , std::size_t   pitch
                   , const mpl::true_&    // bit_aligned
                   )
    {
        BOOST_STATIC_ASSERT(( is_same< View
                                     , typename gray1_image_t::view_t
                                     >::value
                           ));

        byte_vector_t row( pitch / 8 );

        typedef typename View::x_iterator x_it_t;
        x_it_t row_it = x_it_t( &( *row.begin() ));

        negate_bits< byte_vector_t
                   , mpl::true_
                   > neg;

        mirror_bits< byte_vector_t
                   , mpl::true_
                   > mirror;


        for( typename View::y_coord_t y = 0; y < src.height(); ++y )
        {
            std::copy( src.row_begin( y )
                     , src.row_end( y )
                     , row_it
                     );

            mirror( row );
            neg   ( row );

            _out.write( &row.front()
                      , pitch / 8
                      );
        }
    }

    template< typename View >
    void write_data( const View&   src
                   , std::size_t   pitch
                   , const mpl::false_&    // bit_aligned
                   )
    {
		byte_vector_t buf( pitch );

        typedef typename View::value_type pixel_t;
        typedef typename view_type_from_pixel< pixel_t >::type view_t;

        view_t row = interleaved_view( src.width()
                                     , 1
                                     , reinterpret_cast< pixel_t* >( &buf.front() )
                                     , pitch
                                     );

        for( typename View::y_coord_t y = 0
           ; y < src.height()
           ; ++y
           )
		{
            copy_pixels( subimage_view( src
                                      , 0
                                      , (int) y
                                      , (int) src.width()
                                      , 1
                                      )
                       , row
                       );

            _out.write( &buf.front(), pitch );
		}
    }

private:

    Device& _out;
};

struct pnm_write_is_supported
{
    template< typename View >
    struct apply 
        : public is_write_supported< typename get_pixel_type< View >::type
                                   , pnm_tag
                                   >
    {};
};

template< typename Device >
class dynamic_image_writer< Device
                          , pnm_tag
                          >
    : public writer< Device
                   , pnm_tag
                   >
{
    typedef writer< Device
                  , pnm_tag
                  > parent_t;

public:

    dynamic_image_writer( Device& file )
    : parent_t( file )
    {}

    template< typename Views >
    void apply( const any_image_view< Views >& views )
    {
        dynamic_io_fnobj< pnm_write_is_supported
                        , parent_t
                        > op( this );

        apply_operation( views, op );
    }
};

} // detail
} // gil
} // boost

#endif // BOOST_GIL_EXTENSION_IO_JPEG_IO_WRITE_HPP
