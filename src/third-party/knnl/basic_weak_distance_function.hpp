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
 * File created: Thu 13 Apr 2006 08:30:20 CEST
 * Last modified: Sun 26 Nov 2006 08:32:48 CET
 */

#ifndef BASIC_WEAK_DISTANCE_FUNCTION_HPP_INCLUDED
#define BASIC_WEAK_DISTANCE_FUNCTION_HPP_INCLUDED

/**
 * \defgroup distance Generalized distance
 */

/**
 * \file basic_weak_distance_function.hpp
 * \brief File contains template class Basic_weak_distance_function.
 * \ingroup distance
 */

/**
 * \namespace distance
 * \brief namespace contains functors for distance calculation in the data.
 * \ingroup distance
 */
namespace distance
{
    /**
    * \addtogroup distance
    */
    /*\@{*/

    /**
     * Basic_weak_distance_function template class.
     * \param Value_type is a type of values.
     * \param Parameters_type ia a type of parameters.
     */
    template
    <
        typename Value_type,
        typename Result_type
    >
    class Basic_weak_distance_function
    {
    public:

        typedef Value_type value_type;
        typedef Result_type result_type;

        /**
         * Constructor.
         */
        Basic_weak_distance_function()
        {}

        /** Copy constructor. */
        template
        <
            typename Value_type_2,
            typename Result_type_2
        >
        Basic_weak_distance_function
        (
            Basic_weak_distance_function
            <
                Value_type_2,
                Result_type_2
            >
            const & weak_distance_function_
        )
        {}
    };
    /*\@}*/

} // namespace distance

#endif // BASIC_WEAK_DISTANCE_FUNCTION_HPP_INCLUDED
