/*
    Copyright 2007-2008 Andreas Pokorny
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

/*************************************************************************************************/


#ifndef BOOST_GIL_GRAY_ALPHA_HPP_INCLUDED
#define BOOST_GIL_GRAY_ALPHA_HPP_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
/// \file               
/// \brief
/// \author Andreas Pokorny \n
///         
/// \date   2007-2008 \n
///
////////////////////////////////////////////////////////////////////////////////////////

#include <boost/gil/gil_config.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/gil/gray.hpp>
#include <boost/gil/typedefs.hpp>

namespace boost { namespace gil {

typedef mpl::vector2<gray_color_t,alpha_t> gray_alpha_t;

typedef layout<gray_alpha_t> gray_alpha_layout_t;
typedef layout<gray_alpha_layout_t, mpl::vector2_c<int,1,0> > alpha_gray_layout_t;

GIL_DEFINE_BASE_TYPEDEFS(8,  alpha_gray)
GIL_DEFINE_BASE_TYPEDEFS(8s, alpha_gray)
GIL_DEFINE_BASE_TYPEDEFS(16, alpha_gray)
GIL_DEFINE_BASE_TYPEDEFS(16s,alpha_gray)
GIL_DEFINE_BASE_TYPEDEFS(32 ,alpha_gray)
GIL_DEFINE_BASE_TYPEDEFS(32s,alpha_gray)
GIL_DEFINE_BASE_TYPEDEFS(32f,alpha_gray)

GIL_DEFINE_ALL_TYPEDEFS(8,  gray_alpha)
GIL_DEFINE_ALL_TYPEDEFS(8s, gray_alpha)
GIL_DEFINE_ALL_TYPEDEFS(16, gray_alpha)
GIL_DEFINE_ALL_TYPEDEFS(16s,gray_alpha)
GIL_DEFINE_ALL_TYPEDEFS(32 ,gray_alpha)
GIL_DEFINE_ALL_TYPEDEFS(32s,gray_alpha)
GIL_DEFINE_ALL_TYPEDEFS(32f,gray_alpha)

/// \ingroup ColorConvert
/// \brief Gray Alpha to RGBA
template <>
struct default_color_converter_impl<gray_alpha_t,rgba_t> {
    template <typename P1, typename P2>
    void operator()(const P1& src, P2& dst) const {
        get_color(dst,red_t())  =
            channel_convert<typename color_element_type<P2, red_t>::type>(get_color(src,gray_color_t()));
        get_color(dst,green_t())=
            channel_convert<typename color_element_type<P2, green_t>::type>(get_color(src,gray_color_t()));
        get_color(dst,blue_t()) =
            channel_convert<typename color_element_type<P2, blue_t>::type>(get_color(src,gray_color_t()));
        get_color(dst,alpha_t()) =
            channel_convert<typename color_element_type<P2, alpha_t>::type>(get_color(src,alpha_t()));
    }
};

template <>
struct default_color_converter_impl<gray_alpha_t,rgb_t> {
    template <typename P1, typename P2>
    void operator()(const P1& src, P2& dst) const {
        get_color(dst,red_t())  =
            channel_convert<typename color_element_type<P2, red_t>::type>(
                channel_multiply(get_color(src,gray_color_t()),get_color(src,alpha_t()) ) 
                );
        get_color(dst,green_t())  =
            channel_convert<typename color_element_type<P2, green_t>::type>(
                channel_multiply(get_color(src,gray_color_t()),get_color(src,alpha_t()) ) 
                );
        get_color(dst,blue_t())  =
            channel_convert<typename color_element_type<P2, blue_t>::type>(
                channel_multiply(get_color(src,gray_color_t()),get_color(src,alpha_t()) ) 
                );
    }
};

template <>
struct default_color_converter_impl<gray_alpha_t,gray_t> {
    template <typename P1, typename P2>
    void operator()(const P1& src, P2& dst) const {
        get_color(dst,gray_color_t())  =
            channel_convert<typename color_element_type<P2, gray_color_t>::type>(
                channel_multiply(get_color(src,gray_color_t()),get_color(src,alpha_t()) ) 
                );
    }
};

} // gil
} // boost

#endif // BOOST_GIL_GRAY_ALPHA_HPP_INCLUDED
