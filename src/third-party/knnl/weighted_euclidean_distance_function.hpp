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
 * File created: Mon 17 Apr 2006 23:54:20 CEST
 * Last modified: Sun 26 Nov 2006 09:33:07 CET
 */

#ifndef WEIGHTED_EUCLIDEAN_DISTANCE_FUNCTION_HPP_INCLUDED
#define WEIGHTED_EUCLIDEAN_DISTANCE_FUNCTION_HPP_INCLUDED

#include "operators.hpp"
#include "basic_weak_distance_function.hpp"
#include <cassert>

#include "value_type.hpp"

/**
 * \file weighted_euclidean_distance_function.hpp
 * \brief File contains template class Weighted_euclidean_distance_function.
 * \ingroup distance
 */

namespace distance
{
    /**
    * \addtogroup distance
    */
    /*\@{*/

    /**
     * Weighted_euclidean_distance_function template class.
     * \param Value_type is a type of values.
     * \param Parameters_type is a type of parameters (weights) used in weighted Euclidean distance.
     * \f[
     * d (x,y,w) = \sum\limits_{i=0}^{N} w_i\cdot (x_i-y_i)^2
     * \f]
     */
    template
    <
        typename Parameters_type,
        typename Value_type
    >
    class Weighted_euclidean_distance_function
    : public Basic_weak_distance_function
    <
        Value_type,
        Value_type
    >
    {
    private:
        typedef typename Value_type::value_type inner_type;

        /** Parameters. */
        Parameters_type const * parameters;

        /** Parameters size. */
        ::boost::int32_t parameters_size;

    public:
        typedef Parameters_type parameters_type;

        /**
         * Constructor.
         */
        explicit Weighted_euclidean_distance_function ( Parameters_type const * weights )
        : parameters ( weights )
        {
            assert ( parameters != static_cast < Parameters_type * > ( 0 ) );
            parameters_size = parameters->size();
        }

        /**
         * Calculation function.
         * \param x input value for the function.
         * \param y input value for the function.
         * \return square of the weighted Euclidean distance function.
         * \f[
         * d (x,y,w) = \sum\limits_{i=0}^{N} w_i\cdot (x_i-y_i)^2
         * \f]
         * where: w are parameters given in constructor.
         */
        inner_type operator()
        (
            Value_type const & x,
            Value_type const & y
        ) const
        {
            return
            (
                weighted_euclidean_distance_square
                (
                    x.begin(),
                    x.end(),
                    y.begin(),
                    parameters->begin(),
                    static_cast < inner_type const & > (0)
                )
            );
        }

        /** Copy constructor */
        template
        <
            typename Parameters_type_2,
            typename Value_type_2
        >
        Weighted_euclidean_distance_function
        (
            Weighted_euclidean_distance_function
            <
                Parameters_type_2,
                Value_type_2
            >
            const & weighted_euclidean_distance
        )
        : parameters ( weighted_euclidean_distance.parameters )
        {}

        /** Operator= */
        template
        <
            typename Parameters_type_2,
            typename Value_type_2
        >
        Weighted_euclidean_distance_function
        <
            Parameters_type,
            Value_type
        > &
        operator=
        (
            Weighted_euclidean_distance_function
            <
                Parameters_type_2,
                Value_type_2
            >
            const & weighted_euclidean_distance
        )
        {
            // Handle self-assignment:
            if ( this == &weighted_euclidean_distance )
            {
                return *this;
            }

            parameters = weighted_euclidean_distance.parameters;
            parameters_size = weighted_euclidean_distance.parameters_size;

            return *this;
        }

    private:
        /**
         * Function calculates weighted Euclidean distance between two containers.
         * \param begin_1 is a begin iterator for the first container.
         * \param end_1 is an end iterator for the first container.
         * \param begin_2 is a begin iterator for the second container.
         * \param begin_3 is a bagin iterator for the parameters.
         * \param init is an initial value.
         * \result sqare of weighted Euclidean distance.
         * \f[
         * d (x,y,w) = d_0 + \sum\limits_{i=0}^{N} w_i\cdot (x_i-y_i)^2
         * \f]
         * where: x is given by range of Input_iterator_1,
         * y is given through Input_iterator_2,
         * w is given through Input_iterator_3,
         * \f$d_0\f$ is initial value (init).
         */
        inner_type weighted_euclidean_distance_square
        (
            typename Value_type::const_iterator begin_1,
            typename Value_type::const_iterator end_1,
            typename Value_type::const_iterator begin_2,
            typename Parameters_type::const_iterator begin_3,
            inner_type const & init
        ) const
        {
            inner_type tmp_val;
            inner_type result = init;

            for ( ; begin_1 != end_1 ; ++begin_1, ++begin_2, ++begin_3 )
            {
                tmp_val = *begin_1 - *begin_2;
                result = result + *begin_3 * ( tmp_val * tmp_val );
            }
            return result;
        }
    };
    /*\@}*/

} // namespace distance

#endif // WEIGHTED_EUCLIDEAN_DISTANCE_FUNCTION_HPP_INCLUDED

