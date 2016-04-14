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
 * File created: Sun 18 Jun 2006 13:22:04 CEST
 * Last modified: Sun 26 Nov 2006 08:54:24 CET
 */

#ifndef NUMERIC_ITERATOR_HPP_INCLUDED
#define NUMERIC_ITERATOR_HPP_INCLUDED

#include <boost/cstdint.hpp>

/**
 * \file numeric_iterator.hpp
 * \brief File contains template classes for preparing numeric iterators.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Numeric_iterator
     * \brief Class that defines bahavior of the numeric iterator.
     * \param Value_type is a type of value.
     */
    template < typename Value_type >
    struct Numeric_iterator
    {
        typedef Value_type value_type;
        value_type state;

        /**
         * Constructor.
         * \param state_ is a state in the beginning of the work of iterator
         */
        Numeric_iterator ( Value_type state_ )
        : state ( state_ )
        {}
    };

    /**
     * \class Linear_numeric_iterator
     * \brief Class that defines bahavior of the linear numeric iterator.
     * It begins with given value and iterate using given step.
     * \param Value_type is a type of value.
     */
    template < typename Value_type >
    class Linear_numeric_iterator
    : public Numeric_iterator < Value_type >
    {
    public:
        /**
         * Constructor.
         * \param state_ is a begin value.
         * \param step_ is a step value.
         */
        Linear_numeric_iterator (
            Value_type state_ = static_cast < Value_type > ( 0 ),
            Value_type step_ = static_cast < Value_type > ( 1 )
        )
        : Numeric_iterator < Value_type > ( state_ ), step ( step_ )
        {}

        /**
         * Reset state.
         * \param state_ is a new state.
         */
        void reset ( Value_type const & state_ = static_cast < Value_type > ( 0 ) )
        {
            Numeric_iterator < Value_type >::state = state_;
            return;
        }

        /**
         * Method returns state.
         */
        inline Value_type operator() ( void ) const
        {
            return ( Numeric_iterator < Value_type >::state );
        }

        /**
         * Preincrementation.
         */
        inline Linear_numeric_iterator < Value_type > &
        operator++()
        {
            Numeric_iterator < Value_type >::state += step;
            return *this;
        }

        /**
         * Postincrementation.
         */
        inline Linear_numeric_iterator < Value_type >
        operator++ ( int )
        {
            typedef Numeric_iterator < Value_type > Value_numeric_iterator;
            Value_type tmp ( Value_numeric_iterator::state );
            Numeric_iterator < Value_type >::state += step;
            return tmp;
        }

        ///\todo TODO: try to prepare conversion on Value_type
        /*inline const Value_type operator Value_type ( void ) const
        {
            return Numeric_iterator::state;
        }*/

    protected:
        Value_type step;
    };

    typedef Linear_numeric_iterator < ::boost::int32_t > linear_numeric_iterator;
    /*\@}*/

} // namespace neural_net

#endif // NUMERIC_ITERATOR_HPP_INCLUDED

