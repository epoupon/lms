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
 * File created: Wed 10 May 2006 11:16:03 CEST
 * Last modified: Wed 08 Aug 2007 18:21:33 CEST
 */

#ifndef FUNCTORS_HPP_INCLUDED
#define FUNCTORS_HPP_INCLUDED

#include <cmath>

#include "basic_activation_function.hpp"
#include "operators.hpp"
#include "training_functional.hpp"
/**
 * \file functors.hpp
 * \brief File contains template class Basic_function and some other classes derived form that one.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Basic_function
     * \brief Basic class for defining functions.
     * \param Value_type is a type of values.
     */
    template < typename Value_type >
    struct Basic_function
    {
        typedef Value_type value_type;
    };

    /**
     * \class Gauss_function
     * \brief Functor that compute Gauss hat function.
     * \param Value_type is a type of values.
     * \param Scalar_type is a type of scaling factor which multiplies values.
     * \param Exponent_type is a type of exponential factor.
     * \f[
     * y = e ^{-\frac{1}{2}\left (\frac{v}{\sigma}\right)^p}
     * \f]
     */
    template
    <
        typename Value_type,
        typename Scalar_type,
        typename Exponent_type
    >
    class Gauss_function
    : public Basic_function < Value_type >,
    public Basic_activation_function
    <
        typename ::operators::Max_type < Scalar_type, Exponent_type >::type,
        Value_type,
        typename ::operators::Max_type
        <
            typename ::operators::Max_type
            <
                typename ::operators::Max_type < Scalar_type, Exponent_type >::type,
                Value_type
            >::type,
            double
        >::type
    >
    {
    public:

        typedef Scalar_type scalar_type;
        typedef Exponent_type exponent_type;
        typedef Value_type value_type;

        typedef typename ::operators::Max_type
        <
            typename ::operators::Max_type
            <
                typename ::operators::Max_type < Scalar_type, Exponent_type >::type,
                Value_type
            >::type,
            double
        >::type result_type;

        /** Sigma coafficient in the function. */
        Scalar_type sigma;

        /** Exponential factor. */
        Exponent_type exponent;

        /**
         * Constructor.
         * \param sigma_ is sigma coefficient in Gauss hat function.
         * \param exp_ is exponential factor in Gauss hat function.
         */
        Gauss_function ( Scalar_type const & sigma_, Exponent_type const & exp_ )
        : sigma ( sigma_ ), exponent ( exp_ )
        {}

        /**
         * Result of the functor.
         * \param value is a value.
         * \return calcutaled result.
         * \f[
         * y = e ^{-\frac{1}{2}\left (\frac{v}{\sigma}\right)^p}
         * \f]
         * where: v is value.
         */
        result_type operator() ( Value_type const & value ) const
        {
            ::operators::power < result_type, Exponent_type > power_v;

            // calculate result
            return
            (
                ::std::exp
                (
                    - ::operators::inverse ( static_cast < Scalar_type > ( 2 ) )
                    * (power_v)
                    (
                        ::operators::inverse ( sigma ) * value,
                        exponent
                    )
                )
            );
        }

        /** Copy constructor. */
        template
        <
            typename Value_type_2,
            typename Scalar_type_2,
            typename Exponent_type_2
        >
        Gauss_function
        (
            Gauss_function
            <
                Value_type_2,
                Scalar_type_2,
                Exponent_type_2
            >
            const & gauss_function
        )
        : sigma ( gauss_function.sigma ), exponent ( gauss_function.exponent )
        {}
    };

    /**
     * \class Cauchy_function
     * \brief Functor that computes Cauchy hat function.
     * \param Value_type is a type of value.
     * \param Scalar_type is a type of scalar which will multiplies values.
     * \param Power_type is a type of exponential factor.
     * \f[
     * y=\frac{1}{1 + (\frac{x}{\sigma})^p}
     * \f]
     */
    template
    <
        typename Value_type,
        typename Scalar_type,
        typename Exponent_type
    >
    class Cauchy_function
    : public Basic_function < Value_type >,
    public Basic_activation_function
    <
        typename ::operators::Max_type < Scalar_type, Exponent_type >::type,
        Value_type,
        typename ::operators::Max_type
        <
            typename ::operators::Max_type < Scalar_type, Exponent_type >::type,
            Value_type
        >::type
    >
    {
    public:

        typedef Scalar_type scalar_type;
        typedef Exponent_type exponent_type;
        typedef Value_type value_type;

        typedef typename ::operators::Max_type
        <
            typename ::operators::Max_type < Scalar_type, Exponent_type >::type,
            Value_type
        >::type result_type;

        /** Sigma scaling coefficient. */
        Scalar_type sigma;

        /** Exponential factor. */
        Exponent_type exponent;

        /**
         * Constuctor.
         * \param sigma_ is scailing coefficient.
         * \param exp_ is exponential factor.
         */
        Cauchy_function ( Scalar_type const & sigma_, Exponent_type const & exp_ )
        : sigma ( sigma_ ), exponent ( exp_ )
        {}

        /**
         * Function calculates values of the Cauchy hat function.
         * \param value is a value.
         * \return value of the function.
         * \f[
         * y=\frac{1}{1 + (\frac{x}{\sigma})^p}
         * \f]
         * where: x is value.
         */
        result_type operator() ( Value_type const & value ) const
        {
            ::operators::power < result_type, Exponent_type > power_v;

            // calculate result
            return
            (
                ::operators::inverse
                (
                    (power_v)
                    (
                        ::operators::inverse ( sigma ) * value,
                        exponent
                    ) + 1
                )
            );
        }

        /**constructor. */
        template
        <
            typename Value_type_2,
            typename Scalar_type_2,
            typename Exponent_type_2
        >
        Cauchy_function
        (
            Cauchy_function
            <
                Value_type_2,
                Scalar_type_2,
                Exponent_type_2
            >
            const & cauchy_function
        )
        : sigma ( cauchy_function.sigma ), exponent ( cauchy_function.exponent )
        {}
    };

    /**
     * \class Constant_function
     * \brief Functor that computes constant function.
     * \param Value_type is a type of value.
     * \param Scalar_type is a type of constant value.
     * \f[
     * y=c
     * \f]
     */
    template
    <
        typename Value_type,
        typename Scalar_type
    >
    class Constant_function
    : public Basic_function < Value_type >
    {
    public:

        typedef Scalar_type scalar_type;
        typedef Scalar_type result_type;

        /** Sigma constant value, but not const. */
        Scalar_type sigma;

        /**
         * Constuctor.
         * \param sigma_ is constant value.
         */
        Constant_function ( const Scalar_type & sigma_ )
        : sigma ( sigma_ )
        {}

        /** Copy constructor. */
        template
        <
            typename Value_type_2,
            typename Scalar_type_2
        >
        Constant_function
        (
            Constant_function
            <
                Value_type_2,
                Scalar_type_2
            >
            const & constant_function
        )
        : sigma ( constant_function.sigma )
        {}

        /**
         * Constant function.
         * \param value is a value.
         * \return constant value.
         * \f[
         * y=c
         * \f]
         * where: x is value.
         */
        result_type operator() ( Value_type const & //value 
            ) const
        {
            // result
            return ( sigma );
        }
    };
    /*\@}*/

} // namespace neural_net

#endif // FUNCTORS_HPP_INCLUDED

