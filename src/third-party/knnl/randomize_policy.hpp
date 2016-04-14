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
 * File created: Mon 22 May 2006 20:03:59 CEST
 * Last modified: Wed 08 Aug 2007 17:27:14 CEST
 */

#ifndef RANDOMIZE_POLICY_HPP_INCLUDED
#define RANDOMIZE_POLICY_HPP_INCLUDED

#include <cstdlib>
#include <ctime>

/**
 * \file randomize_policy.hpp
 * \brief File contains policy classes to set randomize functionality
 * in to network generation process.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/
    class Randomize_policy
    {
        typedef Randomize_policy this_type;
    };

    /**
     * \class External_randomize
     * \brief This class force not to use srand() in generation proces,
     * therefore srand function is initialized externally.
     */
    struct External_randomize
    : public Randomize_policy
    {
        typedef External_randomize this_type;

        /** Do nothing :-). */
        void operator() ( void ) const
        {
            return;
        }
    };

    /**
     * \class Internal_randomize
     * \brief This class force to use srand() in generation proces,
     * therefore srand function is NOT initialized.
     */
    struct Internal_randomize
    : public Randomize_policy
    {
        typedef Internal_randomize this_type;

        /** Initialize random number generator using srand(). */
        void operator() ( void ) const
        {
            ::std::srand (static_cast<unsigned int> (::std::time (NULL)));
            return;
        }
    };
    /*\@}*/

} // namespace neural_net

#endif // RANDOMIZE_POLICY_HPP_INCLUDED

