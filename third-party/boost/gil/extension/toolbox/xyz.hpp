// Copyright 2008 Chung-Lin Wen.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/*************************************************************************************************/

#ifndef GIL_XYZ_H
#define GIL_XYZ_H

////////////////////////////////////////////////////////////////////////////////////////
/// \file
/// \brief Support for CIE XYZ color space
/// \author Chung-Lin Wen \n
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/cast.hpp>
#include <boost/gil/gil_all.hpp>

namespace boost { namespace gil {

/// \addtogroup ColorNameModel
/// \{
namespace xyz_color_space
{
/// \brief x Color Component
struct x_t {};    
/// \brief y Color Component
struct y_t {};
/// \brief z Color Component
struct z_t {}; 
}
/// \}

/// \ingroup ColorSpaceModel
typedef mpl::vector3< xyz_color_space::x_t
                    , xyz_color_space::y_t
                    , xyz_color_space::z_t
                    > xyz_t;

/// \ingroup LayoutModel
typedef layout<xyz_t> xyz_layout_t;


GIL_DEFINE_ALL_TYPEDEFS( 32f, xyz );

/// \ingroup ColorConvert
/// \brief RGB to XYZ
template <>
struct default_color_converter_impl< rgb_t, xyz_t >
{
   template <typename P1, typename P2>
   void operator()( const P1& src, P2& dst ) const
   {
      using namespace xyz_color_space;

      // only bits32f for xyz is supported
      bits32f temp_red   = channel_convert<bits32f>( get_color( src, red_t()   ));
      bits32f temp_green = channel_convert<bits32f>( get_color( src, green_t() ));
      bits32f temp_blue  = channel_convert<bits32f>( get_color( src, blue_t()  ));

      bits32f normalized_r = temp_red / 255.f;
      bits32f normalized_g = temp_green / 255.f;
      bits32f normalized_b = temp_blue / 255.f;

      if( normalized_r > 0.04045f )
      {
         normalized_r = pow((( normalized_r + 0.055f ) / 1.055f ), 2.4f );
      }
      else
      {
         normalized_r /= 12.92f;
      }

      if( normalized_g > 0.04045f )
      {
         normalized_g = pow((( normalized_g + 0.055f ) / 1.055f ), 2.4f );
      }
      else
      {
         normalized_g /= 12.92f;
      }
      
      if( normalized_b > 0.04045f )
      {
         normalized_b = pow((( normalized_b + 0.055f ) / 1.055f ), 2.4f );
      }
      else
      {
         normalized_b /= 12.92f;
      }

      normalized_r *= 100.f;
      normalized_g *= 100.f;
      normalized_b *= 100.f;

      bits32f x, y, z;
      x = normalized_r * 0.4124f + normalized_g * 0.3576f + normalized_b * 0.1805f;
      y = normalized_r * 0.2126f + normalized_g * 0.7152f + normalized_b * 0.0722f;
      z = normalized_r * 0.0193f + normalized_g * 0.1192f + normalized_b * 0.9505f;

      get_color( dst, x_t() ) = x;
      get_color( dst, y_t() ) = y;
      get_color( dst, z_t() ) = z;
   }
};

/// \ingroup ColorConvert
/// \brief XYZ to RGB
template <>
struct default_color_converter_impl<xyz_t,rgb_t>
{
   template <typename P1, typename P2>
   void operator()( const P1& src, P2& dst) const
   {
      using namespace xyz_color_space;

      bits32f x = get_color( src, x_t() );
      bits32f y = get_color( src, y_t() );
      bits32f z = get_color( src, z_t() );

      bits32f normalized_x = x / 100.f;
      bits32f normalized_y = y / 100.f;
      bits32f normalized_z = z / 100.f;

      bits32f result_r = normalized_x *  3.2406f + normalized_y * -1.5372f + normalized_z * -0.4986f;
      bits32f result_g = normalized_x * -0.9689f + normalized_y *  1.8758f + normalized_z *  0.0415f;
      bits32f result_b = normalized_x *  0.0557f + normalized_y * -0.2040f + normalized_z *  1.0570f;

      if ( result_r > 0.0031308f ) 
      {
         result_r = 1.055f * pow( result_r, 1.f/2.4f ) - 0.055f;
      }
      else                     
      {
         result_r = 12.92f * result_r;
      }

      if ( result_g > 0.0031308f ) 
      {
         result_g = 1.055f * pow( result_g, 1.f/2.4f ) - 0.055f;
      }
      else                     
      {
         result_g = 12.92f * result_g;
      }

      if ( result_b > 0.0031308f ) 
      {
         result_b = 1.055f * pow( result_b, 1.f/2.4f ) - 0.055f;
      }
      else                     
      {
         result_b = 12.92f * result_b;
      }

      bits32f red, green, blue;
      red = result_r * 255.f;
      green = result_g * 255.f;
      blue = result_b * 255.f;

      get_color(dst,red_t())  =
         channel_convert<typename color_element_type<P2, red_t>::type>( red );
      get_color(dst,green_t())=
         channel_convert<typename color_element_type<P2, green_t>::type>( green );
      get_color(dst,blue_t()) =
         channel_convert<typename color_element_type<P2, blue_t>::type>( blue );
   }
};

} }  // namespace boost::gil

#endif // GIL_XYZ_H
