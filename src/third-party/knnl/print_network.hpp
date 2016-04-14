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
 * File created: Fri 21 Apr 2006 19:30:09 CEST
 * Last modified: Wed 08 Aug 2007 17:26:34 CEST
 */

#ifndef PRINT_NETWORK_HPP_INCLUDED
#define PRINT_NETWORK_HPP_INCLUDED

#include <iostream>
#include <iterator>
#include <algorithm>

/**
 * \file print_network.hpp
 * \brief File contains template function to print network.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
     * \addtogroup neural_net
     */
    /*\@{*/

    /**
     * Function prints weight of the neural network.
     * \param os is output stream.
     * \param network is a reference to network.
     * \return modified stream.
     */
    template < typename T >
    ::std::ostream & print_network_weights
    ( ::std::ostream & os, T const & network, char const * sep = "\t" )
    {
        const typename T::row_size_t M = network.objects.size();
        const typename T::col_size_t N = network.objects[0].size();

        for ( typename T::row_size_t i = 0; i < M; ++i )
        {
            for ( typename T::col_size_t j = 0; j < N; ++j )
            {
                ::std::copy
                    (
                        network.objects[i][j].weights.begin(),
                        network.objects[i][j].weights.end(),
                        ::std::ostream_iterator
                        <
                        typename T::value_type::weights_type::value_type
                        > ( os, sep )
                    );
                os << ::std::endl;
            }
        }
        return os;
    }

    /**
     * Function puts container of any values to the stream.
     * \param os is a output stream.
     * \param value is a value.
     * \return modified stream.
     */
    template < typename T >
    inline ::std::ostream & container_to_ostream ( ::std::ostream & os, T const & container )
    {
        typedef typename T::value_type Value;
        ::std::copy ( container.begin(), container.end(), ::std::ostream_iterator < Value > ( os, " " ) );
        return os;
    }

    /**
     * Function prints structure and results of the neural network.
     * \param os is output stream.
     * \param network is a reference to network.
     * \param value is a value that network will calculate results.
     * \return modified stream.
     */
    template < typename T, typename U >
    ::std::ostream & print_network ( ::std::ostream & os, T const & network, U const & value )
    {
        const typename T::row_size_t M = network.objects.size();
        const typename T::col_size_t N = network.objects[0].size();

        for ( typename T::row_size_t i = 0; i < M; ++i )
        {
            for ( typename T::col_size_t j = 0; j < N; ++j )
            {
                os << "weights[" << i <<"][" << j << "] = ";
                ::std::copy
                    (
                        network.objects[i][j].weights.begin(),
                        network.objects[i][j].weights.end(),
                        ::std::ostream_iterator
                        <
                        typename T::value_type::weights_type::value_type
                        > ( os, "\t" )
                    );
                os << " ( ";
                
                container_to_ostream ( os, value );
                
                os << " ) == ";
                //os << network.objects[i][j] ( value );
                //os << " == ";
                os << network.objects [ i ][ j ]( value );
                os << ::std::endl;
            }
            os << ::std::endl;
        }
        return os;
    }
    /*\@}*/
}// namespace neural_net

#endif // PRINT_NETWORK_HPP_INCLUDED

