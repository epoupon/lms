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
 * File created: Tue 11 Apr 2006 17:47:44 CEST
 * Last modified: Wed 08 Aug 2007 18:29:02 CEST
 */

#ifndef OPERATORS_HPP_INCLUDED
#define OPERATORS_HPP_INCLUDED

#include <cmath>
#include <functional>
#include <algorithm>

#include <boost/type_traits.hpp>

#include "max_type.hpp"

/**
 * \defgroup operators Operators
 */

/**
 * \file operators.hpp
 * \brief File contains template operators.
 * \ingroup operators
 */

/**
 * \namespace operators
 * \brief Operators.
 * \ingroup operators
 */

/** \addtogroup operators */
/*\@{*/
namespace operators
{
    /**
     * Absolute function.
     * \param value is value.
     * \return absolute value.
     */
    template < typename T >
    inline T abs ( T const & value )
    {
        return ( value > 0 ? value : -value );
    }

    /**
     * \class compose_f_gxy_gxy_t
     * \brief Adaptator class compose_f_gxy_gxy_t.
     * \param OP1 is a type of first operator f.
     * \param OP2 is a type of second operator g.
     * \f[
     * y=f (g (x,y),g (x,y))
     * \f]
     */
    template < typename OP1, typename OP2 >
    class compose_f_gxy_gxy_t
    : public ::std::binary_function
    <
        typename OP2::first_argument_type,
        typename OP2::second_argument_type,
        typename OP1::result_type
    >
    {
    public:
        /**
         * Constructor.
         * \param o1 is a reference to the f operator.
         * \param o2 is a reference to the g operator.
         */
        compose_f_gxy_gxy_t
        (
            OP1 const & o1,
            OP2 const & o2
        )
        : op1 ( o1 ), op2 ( o2 )
        {}

        /**
         * Function calculate results.
         * \param x is first argument.
         * \param y is second argument.
         * \f[
         * y=f (g (x,y),g (x,y))
         * \f]
         * where: f is OP1 type, g is OP2 type
         */
        typename OP1::result_type
        operator()
        (
            typename OP2::first_argument_type const & x,
            typename OP2::second_argument_type const & y
        ) const
        {
            return op1 ( op2 ( x, y ),op2 ( x, y ) );
        }

    private:
        /** First operator f. */
        OP1 op1; // calculate: op1 (op2 (x,y),op2 (x,y))

        /** Secong operator g. */
        OP2 op2;
    };

    /**
     * Useful function for creating adaptator compose_f_gxy_gxy.
     * \param o1 is first operator.
     * \param o2 is send operator.
     * \return composition of the operators.
     */
    template < class OP1, class OP2 >
    inline compose_f_gxy_gxy_t < OP1, OP2 >
    compose_f_gxy_gxy
    (
        OP1 const & o1,
        OP2 const & o2
    )
    {
        return compose_f_gxy_gxy_t < OP1, OP2 > ( o1, o2 );
    }

    /**
     * Overloading operator+ for containers.
     * \param lhs is a reference to container x.
     * \param rhs is a reference to container y.
     * \return container.
     * \f[
     * v_i = x_i + y_i
     * \f]
     * where: x is lhs and y is rhs.
     */
    template
    <
        typename T,
        template < typename > class Alloc_type,
        template < typename, typename > class CONT
    >
    CONT < T, Alloc_type <T> >
    operator+
    (
        CONT < T, Alloc_type <T> > const & lhs,
        CONT < T, Alloc_type <T> > const & rhs
    )
    {
        CONT < T, Alloc_type <T> > result ( lhs );

        ::std::transform
        (
            result.begin(),
            result.end(),
            rhs.begin(),
            result.begin(),
            ::std::plus < typename CONT < T, Alloc_type<T> >::value_type >()
        );

        return result;
    }

    /**
     * Overloading operator- for containers.
     * \param lhs is a reference to container x.
     * \param rhs is a reference to container y.
     * \return container.
     * \f[
     * v_i = x_i - y_i
     * \f]
     * where: x is lhs and y is rhs.
     */
    template
    <
        typename T,
        template < typename > class Alloc_type,
        template < typename, typename > class CONT
    >
    CONT < T, Alloc_type <T> >
    operator-
    (
        CONT < T, Alloc_type <T> > const & lhs,
        CONT < T, Alloc_type <T> > const & rhs
    )
    {
        CONT < T , Alloc_type <T> > result ( lhs );

        ::std::transform
        (
            result.begin(),
            result.end(),
            rhs.begin(),
            result.begin(),
            ::std::minus < typename CONT < T , Alloc_type <T> >::value_type >()
        );

        return result;
    }

    /**
     * Overloading operator* for container as product of the scalar value and container.
     * \param a is a reference to container x.
     * \param rhs is a reference to container y.
     * \return container.
     * \f[
     * v_i = a * y_i
     * \f]
     * where: a is scaling coefficient and y is rhs.
     */
    template
    <
        typename K,
        typename T,
        template < typename > class Alloc_type,
        template < typename, typename > class CONT
    >
    CONT < T, Alloc_type <T> >
    operator*
    (
        K const & a,
        CONT < T, Alloc_type <T> > const & rhs
    )
    {
        CONT < T , Alloc_type <T> > result ( rhs );

        ::std::transform
        (
            result.begin(),
            result.end(),
            result.begin(),
            ::std::bind2nd
            (
                ::std::multiplies < typename CONT < T , Alloc_type <T> >::value_type >(),
                a
            )
        );

        return result;
    }

    /**
     * Template function calculates inverse of the value.
     * It could be overloaded/specialized for matrix
     * and other complicated types.
     * \param x is a value to be inversed.
     */
    template < typename Value_type >
    inline typename Max_type < double, Value_type >::type
    inverse ( Value_type const & x )
    {
        typedef typename Max_type < double, Value_type >::type internal_type;

        return
        (
            static_cast < internal_type > ( 1 )
            / static_cast < internal_type > ( x )
        );
    }

    /**
     * \class power
     * \brief Helper class for calculating power.
     * \param T is value type.
     * \param E is exponent type.
     */
    template
    <
        typename T,
        typename E,
        bool ISINTEGRAL = ::boost::is_integral<E>::value
    >
    class power;

    /**
     * Specialization for the integral exponents.
     * \param T is value type.
     * \param E is exponent type.
     * \f[
     * y=v^e
     * \f]
     */
    template < typename T, typename E >
    class power < T, E, true >
    {
    public:
        typedef typename Max_type < T, E >::type result_type;

        result_type operator() ( T const & value_, E const & exp_ ) const
        {
            if ( exp_ == 0 )
            {
                return static_cast < result_type > ( 1 );
            }

            if ( exp_ < 0 )
            {
                return power_int ( value_, -exp_ );
            }
            else
            {
                return power_int ( value_, exp_ );
            }
        }

    private:
        /**
         * Fast power algorithm.
         * \param value_ value.
         * \param exp_ exponent factor.
         * \return value of power
         * \f[
         * z=x^y
         * \f]
         * where: x is value_, y is exp_.
         */
        result_type power_int ( T const & value_, E const & exp_ ) const
        {
            T z = value_;
            result_type y;
            E m = exp_;

            while ( ! ( m & 1 ) )
            {
                m = m / 2;
                z = z * z;
            }
            y = z;

            while ( m > 1 )
            {
                m = m / 2;
                z = z * z;
                if ( m & 1 )
                {
                    y = y * z;
                }
            }
            return y;
        }
    };

    /**
     * Specialization for the real exponents.
     * \param T is value type.
     * \param E is exponent type.
     * \f[
     * y=v^e
     * \f]
     */
    template < typename T, typename E >
    class power < T, E, false >
    {
    public:
        typedef typename Max_type < T, E >::type result_type;

        /**
         * Fast power algorithm.
         * \param value_ value.
         * \param exp_ exponent factor.
         * \return value of power
         * \f[
         * z=x^y
         * \f]
         * where: x is value_, y is exp_.
         */
        result_type operator() ( T const & value_, E const & exp_ ) const
        {
            return ::std::pow ( static_cast < result_type > ( value_ ), exp_ );
        }
    };

    template < typename T, ::boost::int32_t N >
    struct static_power_t;

    template < typename T >
    struct static_power_t<T,0>
    {
        T operator()(T const)
        {
            return static_cast<T>(1);
        }
    };

    template < typename T, ::boost::int32_t N >
    struct static_power_t
    {
        T operator()( T const x )
        {
            //static_power_t<T,N-1> sp;
            return x * static_power_t<T,N-1>()(x);
        }
    };

    template < typename T, ::boost::int32_t N >
    T static_power ( T const x )
    {
        return static_power_t<T,N>()(x);
    }

} // namespace operators
/*\@}*/
#endif // OPERATORS_HPP_INCLUDED

