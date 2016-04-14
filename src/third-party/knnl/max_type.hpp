/*
 * Copyright (c) 2006, Seweryn Habdank-Wojewodzki
 * Copyright (c) 2006, Janusz Rybarski
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the
 * following disclaimer.
 *
 * Redistributions in binary form must reproduce the
 * above copyright notice, this list of conditions
 * and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * e-mail: habdank AT gmail DOT com
 * e-mail: janusz.rybarski AT gmail DOT com
 *
 * File created: Tue 23 May 2006 23:53:14 CEST
 * Last modified: Wed 08 Aug 2007 17:25:27 CEST
 */

#ifndef MAX_TYPE_HPP_INCLUDED
#define MAX_TYPE_HPP_INCLUDED

#include <boost/type_traits.hpp>
#include <boost/cstdint.hpp>

/**
 * \file max_type.hpp
 * \brief File contains template Max_type class.
 * \ingroup operators
 */

/** \addtogroup operators */
/*\@{*/
namespace operators
{
    template < typename S, typename T >
    struct Max_type_private
    {};

    template < typename T >
    struct Max_type_private < T, T >
    {
        typedef T type;
    };

    // just some examples to set up results for Max_type
    // if anyone would like to add his/her types they just have to use
    // macro MAX_TYPE_3( argument_1_type, argument_2_type, result_type )
    #define MAX_TYPE_3(T1,T2,T3) template <>\
    struct Max_type_private < T1, T2 >\
    { typedef T3 type; };

    MAX_TYPE_3(::boost::int32_t,double,double)
    MAX_TYPE_3(short,::boost::int32_t,::boost::int32_t)
    MAX_TYPE_3(int,long,long)
    MAX_TYPE_3(short,double,double)
    MAX_TYPE_3(unsigned char,double,double)
    MAX_TYPE_3(unsigned int,double,double)
    MAX_TYPE_3(unsigned long,double,double)
    MAX_TYPE_3(unsigned short,double,double)
    // and example where we can explicitly use three different types
    // MAX_TYPE(::std::complex < int >, double, ::std::complex < double >)

    // macro below assumes that bigger type is the first one
    // macro MAX_TYPE_2( argument_1_type, argument_2_type )
    // simulate that result_type =  argument_1_type
    #define MAX_TYPE_2(T1,T2) template <>\
    struct Max_type_private < T1, T2 >\
    { typedef T1 type; };

    MAX_TYPE_2(double,::boost::int32_t)
    MAX_TYPE_2(long,int)
    MAX_TYPE_2(long,short)
    MAX_TYPE_2(double,short)
    MAX_TYPE_2(double,unsigned char)
    MAX_TYPE_2(double,unsigned int)
    MAX_TYPE_2(double,unsigned long)
    MAX_TYPE_2(double,unsigned short)

    /**
     * \class Max_type
     * \brief Template that estimates maximal of the mathematical types.
     * \param T_1 first type.
     * \param T_2 second type.
     * \return type e.g. typeid (typename Max_type <double, ::boost::int32_t>::type) == typeid (double),
     * typeid (typename Max_type <::std::complex<::boost::int32_t>,double>::type) == typeid (::std::complex<double>).
     */
    template
    <
        typename T_1,
        typename T_2
    >
    class Max_type
    {
    private:
        typedef typename ::boost::remove_all_extents < T_1 >::type T_1_t;
        typedef typename ::boost::remove_all_extents < T_2 >::type T_2_t;

    public:
        typedef typename Max_type_private < T_1_t, T_2_t >::type type;
    };

    //#undef MAX_TYPE_2
    //#undef MAX_TYPE_3

} // namespace operators
/*\@}*/
#endif // MAX_TYPE_HPP_INCLUDED

