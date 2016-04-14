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
 * File created: Fri 14 Apr 2006 22:42:31 CEST
 * Last modified: Wed 08 Aug 2007 18:21:06 CEST
 */

#ifndef EUCLIDEAN_DISTANCE_FUNCTION_HPP_INCLUDED
#define EUCLIDEAN_DISTANCE_FUNCTION_HPP_INCLUDED

#include <numeric>

#include "operators.hpp"
#include "basic_weak_distance_function.hpp"
#include "value_type.hpp"

/**
 * \file euclidean_distance_function.hpp
 * \brief File contains template class Euclidean_distance_function.
 * \ingroup distance
 */

namespace distance
{
    /**
     * \addtogroup distance
     */
    /*\@{*/

    /**
     * Euclidean_distance_function template class.
     * \param Value_type is a type of values.
     * \f[
     * d (x,y) = \sum\limits_{i=0}^{N} cdot (x_i-y_i)^2
     * \f]
     */
    template
    <
        typename Value_type
    >
    class Euclidean_distance_function
    : public Basic_weak_distance_function
    <
        Value_type,
        Value_type
    >
    {
    public:
        /**
         * Constructor.
         */
        Euclidean_distance_function()
        {}

        /**
         * Calculation function.
         * \param x first input value for the function.
         * \param y second input value for the function.
         * \return square of the Euclidean distance function.
         * \f[
         * d (x,y) = \sum\limits_{i=0}^{N} cdot (x_i-y_i)^2
         * \f]
         */
        typename Value_type::value_type operator()
        (
            Value_type const & x,
            Value_type const & y
        ) const
        {
            return
            (
                euclidean_distance_square
                (
                    x.begin(),
                    x.end(),
                    y.begin(),
                    static_cast < typename Value_type::value_type const & > ( 0 )
                )
            );
        }

    private:
        typedef typename Value_type::value_type inner_type;

        /**
        * Function calculates Euclidean distance between two containers.
        * \param begin_1 is a begin iterator for the first container.
        * \param end_1 is an end iterator for the first container.
        * \param begin_2 is an begin iterator for the second container.
        * \param init is an initial value.
        * \result sqare of Euclidean distance.
        */
        inner_type euclidean_distance_square
        (
            typename Value_type::const_iterator begin_1,
            typename Value_type::const_iterator end_1,
            typename Value_type::const_iterator begin_2,
            inner_type const & init
        ) const
        {
            return ::std::inner_product
            (
                begin_1, end_1, begin_2,
                static_cast < inner_type > ( init ),
                ::std::plus < inner_type >(),
                ::operators::compose_f_gxy_gxy
                (
                    ::std::multiplies < inner_type >(),
                    ::std::minus < inner_type >()
                )
            );
        }

    };
    /*\@}*/

} // namespace distance

#endif // EUCLIDEAN_DISTANCE_FUNCTION_HPP_INCLUDED

