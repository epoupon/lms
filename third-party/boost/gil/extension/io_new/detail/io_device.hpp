/*
    Copyright 2007-2008 Andreas Pokorny, Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/

#ifndef BOOST_GIL_EXTENSION_IO_DETAIL_IO_DEVICE_HPP
#define BOOST_GIL_EXTENSION_IO_DETAIL_IO_DEVICE_HPP

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief
/// \author Andreas Pokorny, Christian Henning \n
///
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <cstdio>

#include <boost/shared_ptr.hpp>

#include <boost/utility/enable_if.hpp>
#include "base.hpp"

namespace boost { namespace gil { namespace detail {

template < typename T > struct buff_item
{
    static const unsigned int size = sizeof( T );
};

template <> struct buff_item< void >
{
    static const unsigned int size = 1;
};


/*!
 * Implements the IODevice concept c.f. to \ref IODevice required by Image libraries like
 * libjpeg and libpng.
 *
 * \todo switch to a sane interface as soon as there is
 * something good in boost. I.E. the IOChains library
 * would fit very well here.
 *
 * This implementation is based on FILE*.
 */
template< typename FormatTag >
class file_stream_device
{
public:

   typedef FormatTag _tag_t;

public:
    struct read_tag {};
    struct write_tag {};

    file_stream_device( std::string const& file_name, read_tag )
        : file(0),_close(true)
    {
        io_error_if( ( file = fopen( file_name.c_str(), "rb" )) == NULL
                , "file_stream_device: failed to open file" );
    }

    file_stream_device( std::string const& file_name, write_tag )
        : file(0),_close(true)
    {
        io_error_if( ( file = fopen( file_name.c_str(), "wb" )) == NULL
                , "file_stream_device: failed to open file" );
    }

    file_stream_device( FILE * filep)
        : file(filep),_close(false)
    {
    }

    ~file_stream_device()
    {
        if(_close)
            fclose(file);
    }

    int getc_unchecked()
    {
        return std::getc( file );
    }

    char getc()
    {
        int ch;

        if(( ch = std::getc( file )) == EOF )
            io_error( "file_stream_device: unexpected EOF" );

        return ( char ) ch;
    }

    std::size_t read( byte_t*     data
                    , std::size_t count )
    {
        return fread( data, 1, static_cast<int>( count ), file );
    }

    /// Reads array
    template< typename T
            , int      N
            >
    size_t read( T (&buf)[N] )
    {
	    return read( buf, N );
    }

    /// Reads byte
    uint8_t read_int8() throw()
    {
        byte_t m[1];

        read( m );
        return m[0];
    }

    /// Reads 16 bit little endian integer
    uint16_t read_int16() throw()
    {
        byte_t m[2];

        read( m );
        return (m[1] << 8) | m[0];
    }

    /// Reads 32 bit little endian integer
    uint32_t read_int32() throw()
    {
        byte_t m[4];

        read( m );
        return (m[3] << 24) | (m[2] << 16) | (m[1] << 8) | m[0];
    }

    /// Writes number of elements from a buffer
    template < typename T >
    size_t write(const T *buf, size_t cnt) throw()
    {
        return fwrite( buf, buff_item<T>::size, cnt, file );
    }

    /// Writes array
    template < typename T
             , size_t   N
             >
    size_t write( const T (&buf)[N] ) throw()
    {
        return write( buf, N );
    }

    /// Writes byte
    void write_int8( uint8_t x ) throw()
    {
	    byte_t m[1] = { x };
	    write(m);
    }

    /// Writes 16 bit little endian integer
    void write_int16( uint16_t x ) throw()
    {
	    byte_t m[2];

	    m[0] = byte_t( x >> 0 );
	    m[1] = byte_t( x >> 8 );

	    write( m );
    }

    /// Writes 32 bit little endian integer
    void write_int32( uint32_t x ) throw()
    {
	    byte_t m[4];

	    m[0] = byte_t( x >>  0 );
	    m[1] = byte_t( x >>  8 );
	    m[2] = byte_t( x >> 16 );
	    m[3] = byte_t( x >> 24 );

	    write( m );
    }


    //!\todo replace with std::ios::seekdir?
    void seek( long count, int whence = SEEK_SET )
    {
        fseek(file, count, whence );
    }

    void flush()
    {
        fflush( file );
    }

    /// Prints formatted ASCII text
    void print_line( const std::string& line )
    {
        fwrite( line.c_str(), sizeof( char ), line.size(), file );
    }

private:

    file_stream_device( file_stream_device const& );
    file_stream_device& operator=( file_stream_device const& );

    //@todo: why not fstream?
    FILE* file;

    bool _close;

};


/**
 * Input stream device
 */
template< typename FormatTag >
class istream_device
{
public:
   istream_device( std::istream& in )
   : _in( in ) {}

    int getc_unchecked()
    {
        return _in.get();
    }

    char getc()
    {
        int ch;

        if(( ch = _in.get() ) == EOF )
            io_error( "file_stream_device: unexpected EOF" );

        return ( char ) ch;
    }

    std::size_t read( byte_t*     data
                    , std::size_t count )
    {
        std::streamsize cr = 0;

        do
        {
            _in.peek();
            std::streamsize c = _in.readsome( reinterpret_cast< char* >( data )
                                            , static_cast< std::streamsize >( count ));

            count -= static_cast< std::size_t >( c );
            data += c;
            cr += c;

        } while( count && _in );

        return static_cast< std::size_t >( cr );
    }

    /// Reads array
    template< typename T
            , int      N
            >
    size_t read( T (&buf)[N] )
    {
	    return read( buf, N );
    }

    /// Reads byte
    uint8_t read_int8() throw()
    {
        byte_t m[1];

        read( m );
        return m[0];
    }

    /// Reads 16 bit little endian integer
    uint16_t read_int16() throw()
    {
        byte_t m[2];

        read( m );
        return (m[1] << 8) | m[0];
    }

    /// Reads 32 bit little endian integer
    uint32_t read_int32() throw()
    {
        byte_t m[4];

        read( m );
        return (m[3] << 24) | (m[2] << 16) | (m[1] << 8) | m[0];
    }

    void seek( long count, int whence = SEEK_SET )
    {
        _in.seekg( count
                 , whence == SEEK_SET ? std::ios::beg
                                      :( whence == SEEK_CUR ? std::ios::cur
                                                            : std::ios::end )
                 );
    }

    void write( const byte_t* data
              , std::size_t   count )
    {
        io_error( "Bad io error." );
    }

    void flush() {}

private:

    std::istream& _in;
};

/**
 * Output stream device
 */
template< typename FormatTag >
class ostream_device
{
public:
    ostream_device( std::ostream & out )
        : _out( out )
    {
    }

    size_t read( byte_t*     data
               , std::size_t count )
    {
        io_error( "Bad io error." );

        return 0;
    }

    void seek( long count, int whence )
    {
        _out.seekp( count
                  , whence == SEEK_SET
                    ? std::ios::beg
                    : ( whence == SEEK_CUR
                        ?std::ios::cur
                        :std::ios::end )
                  );
    }

    void write( const byte_t* data
              , std::size_t   count )
    {
        _out.write( reinterpret_cast<char const*>( data )
                 , static_cast<std::streamsize>( count )
                 );
    }

    /// Writes array
    template < typename T
             , size_t   N
             >
    void write( const T (&buf)[N] ) throw()
    {
        write( buf, N );
    }

    /// Writes byte
    void write_int8( uint8_t x ) throw()
    {
	    byte_t m[1] = { x };
	    write(m);
    }

    /// Writes 16 bit little endian integer
    void write_int16( uint16_t x ) throw()
    {
	    byte_t m[2];

	    m[0] = byte_t( x >> 0 );
	    m[1] = byte_t( x >> 8 );

	    write( m );
    }

    /// Writes 32 bit little endian integer
    void write_int32( uint32_t x ) throw()
    {
	    byte_t m[4];

	    m[0] = byte_t( x >>  0 );
	    m[1] = byte_t( x >>  8 );
	    m[2] = byte_t( x >> 16 );
	    m[3] = byte_t( x >> 24 );

	    write( m );
    }

    void flush()
    {
        _out << std::flush;
    }

    /// Prints formatted ASCII text
    void print_line( const std::string& line )
    {
        _out << line;
    }



private:

    std::ostream& _out;
};


/**
 * Metafunction to detect input devices.
 * Should be replaced by an external facility in the future.
 */
template< typename IODevice  > struct is_input_device : mpl::false_{};
template< typename FormatTag > struct is_input_device< file_stream_device< FormatTag > > : mpl::true_{};
template< typename FormatTag > struct is_input_device<     istream_device< FormatTag > > : mpl::true_{};

template< typename FormatTag
        , typename T
        , typename D = void
        >
struct is_adaptable_input_device : mpl::false_{};

template< typename FormatTag
        , typename T
        >
struct is_adaptable_input_device< FormatTag
                                , T
                                , typename enable_if< mpl::or_< is_base_and_derived< std::istream, T >
                                                              , is_same            < std::istream, T >
                                                              >
                                                    >::type
                                > : mpl::true_
{
    typedef istream_device< FormatTag > device_type;
};

template< typename FormatTag >
struct is_adaptable_input_device< FormatTag
                                , FILE*
                                , void
                                >
    : mpl::true_
{
    typedef file_stream_device< FormatTag > device_type;
};



/**
 * Metafunction to detect output devices.
 * Should be replaced by an external facility in the future.
 */
template<typename IODevice> struct is_output_device : mpl::false_{};

template< typename FormatTag > struct is_output_device< file_stream_device< FormatTag > > : mpl::true_{};
template< typename FormatTag > struct is_output_device< ostream_device    < FormatTag > > : mpl::true_{};

template< typename FormatTag
        , typename IODevice
        , typename D = void
        >
struct is_adaptable_output_device : mpl::false_ {};

template< typename FormatTag
        , typename T
        > struct is_adaptable_output_device< FormatTag
                                           , T
                                           , typename enable_if< mpl::or_< is_base_and_derived< std::ostream, T >
                                                                         , is_same            < std::ostream, T >
                                                                         >
                                           >::type
        > : mpl::true_
{
    typedef ostream_device< FormatTag > device_type;
};

template<typename FormatTag> struct is_adaptable_output_device<FormatTag,FILE*,void>
  : mpl::true_
{
    typedef file_stream_device< FormatTag > device_type;
};

template< typename Device, typename FormatTag, typename ConversionPolicy > class reader;
template< typename Device, typename FormatTag, typename Log = no_log > class writer;

template< typename Device, typename FormatTag > class dynamic_image_reader;
template< typename Device, typename FormatTag, typename Log = no_log > class dynamic_image_writer;
} // namespace detail
} // namespace gil
} // namespace boost

#endif // BOOST_GIL_EXTENSION_IO_DETAIL_IO_DEVICE_HPP
