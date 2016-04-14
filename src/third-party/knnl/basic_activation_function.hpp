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
 * Last modified: Mon 22 May 2006 13:28:47 CEST
 */

#ifndef BASIC_ACTIVATION_FUNCTION_HPP_INCLUDED
#define BASIC_ACTIVATION_FUNCTION_HPP_INCLUDED

/**
 * \file basic_activation_function.hpp
 * \brief File contains template class Basic_activation_function.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
     * \addtogroup neural_net
     */
    /*\@{*/

    /**
     * Basic_activation_function template class.
     * \param Parameters_type is type of parameters.
     * \param Value_type is a type of values.
     * \param Result_type is a type of results.
     */
    template
    <
        typename Parameters_type,
        typename Value_type,
        typename Result_type
    >
    class Basic_activation_function
    {
    public:
        /** Result type. */
        typedef Result_type result_type;

        /** Value type. */
        typedef Value_type value_type;

        /** Parameters type. */
        typedef Parameters_type parameters_type;
    };
    /*\@}*/

} // namespace neural_net

#endif // BASIC_ACTIVATION_FUNCTION_HPP_INCLUDED
