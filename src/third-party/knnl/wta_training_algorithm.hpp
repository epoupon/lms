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
 * File created: Fri 21 Apr 2006 17:33:34 CEST
 * Last modified: Wed 08 Aug 2007 18:29:31 CEST
 */

#ifndef WTA_TRAINING_ALGORITM_HPP_INCLUDED
#define WTA_TRAINING_ALGORITM_HPP_INCLUDED

#include <cassert>
#include <algorithm>
#include <limits>
#include <iterator>

#include <boost/bind.hpp>

#include "training_functional.hpp"
#include "numeric_iterator.hpp"

/**
 * \file wta_training_algorithm.hpp
 * \brief File contains template class Wta_training_algoritm.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Wta_training_algorithm
     * \brief Class contains functionality for training
     * kohonen network using WTA method.
     * \param Network_type is a network type.
     * \param Value_type is a type os single data
     * in mathematical meanning, so it could be ::std::vector<double>, too.
     * \param Data_iterator_type is is iterator for container
     * with training data.
     * \param Training_functional_type is a type of functional.
     * \param Numeric_iterator_type is a type of numeric iterator.
     */
    template
    <
        typename Network_type,
        typename Value_type,
        typename Data_iterator_type,
        typename Training_functional_type,
        typename Numeric_iterator_type
            = Linear_numeric_iterator <
                typename Training_functional_type::iteration_type
            >
    >
    class Wta_training_algorithm
    {
    public:

        typedef typename Training_functional_type::iteration_type iteration_type;
        typedef Numeric_iterator_type numeric_iterator_type;
        typedef Network_type network_type;
        typedef Value_type value_type;
        typedef Data_iterator_type data_iterator_type;
        typedef Training_functional_type training_functional_type;

        /** Training functional. */
        Training_functional_type training_functional;

        /**
         * Constructor.
         * \param training_functional_ is a training functor.
         * \param numeric_iterator_ is a numeric iterator.
         */
        Wta_training_algorithm
        (
            Training_functional_type const & training_functional_,
            Numeric_iterator_type numeric_iterator_ = linear_numeric_iterator()
        )
        : training_functional ( training_functional_ ),
        numeric_iterator ( numeric_iterator_ )
        {
            network = static_cast < Network_type * > ( 0 );
        }

        /**
         * Copy constructor.
         * It makes flat copy of neural network, so it copies only pointer not structure.
         */
        template
        <
            typename Network_type_2,
            typename Value_type_2,
            typename Data_iterator_type_2,
            typename Training_functional_type_2,
            typename Numeric_iterator_type_2
        >
        Wta_training_algorithm
        (
            Wta_training_algorithm
            <
                Network_type_2,
                Value_type_2,
                Data_iterator_type_2,
                Training_functional_type_2,
                Numeric_iterator_type_2
            >
            const & wta_training_alg_
        )
        : training_functional ( wta_training_alg_.training_functional ),
        numeric_iterator ( wta_training_alg_.numeric_iterator ),
        iteration ( wta_training_alg_.iteration )
        {
            network = wta_training_alg_.network;
        }

        /**
         * Function that starts training proces.
         * \param network_ is a pointer to the existing kohonen neural network.
         * \param data_begin is a begin iterator, it could be revers.
         * \param data_end is end iterator.
         * \return error code.
         */
        ::boost::int32_t operator()
        (
            Data_iterator_type data_begin,
            Data_iterator_type data_end,
            Network_type * network_
        )
        {
            network = network_;

            // check if pointer is not null
            assert ( network != static_cast < Network_type * > ( 0 ) );

            // for each data train neural network
            ::std::for_each
            (
                data_begin, data_end,
                ::boost::bind
                (
                    & Wta_training_algorithm
                    <
                        Network_type,
                        Value_type,
                        Data_iterator_type,
                        Training_functional_type,
                        Numeric_iterator_type
                    >::train,
                    this,
                    _1
                )
            );

            return 0;
        }

    protected:
        /** Pointer to the network. */
        Network_type * network;

        iteration_type iteration;

        Numeric_iterator_type numeric_iterator;

        /**
         * Function trains neural network using single value.
         * \param value is a value.
         * As is set in WTA algoritm method is looking for the best neuron,
         * and train it to have better results with actual data in the future.
         */
        void train ( Value_type const & value )
        {
            typename Network_type::row_size_t index_1 = 0;
            typename Network_type::col_size_t index_2 = 0;

            typename Network_type::value_type::result_type tmp_result;

            // reset max_result
            typename Network_type::value_type::result_type max_result
                = ::std::numeric_limits
                <
                    typename Network_type::value_type::result_type
                >::min();

            typename Network_type::row_iterator r_iter;
            typename Network_type::column_iterator c_iter;

            // set ranges for iteration procedure
            typename Network_type::row_size_t r_counter = 0;
            typename Network_type::col_size_t c_counter = 0;

            for ( r_iter = network->objects.begin();
                  r_iter != network->objects.end();
                  ++r_iter
            )
            {
                for ( c_iter = r_iter->begin();
                      c_iter != r_iter->end();
                      ++c_iter
                )
                {
                    tmp_result = ( *c_iter ) ( value );
                    if ( tmp_result > max_result )
                    {
                        index_1 = r_counter;
                        index_2 = c_counter;
                        max_result = tmp_result;
                    }
                    ++c_counter;
                }
                ++r_counter;
                c_counter = 0;
            }

            r_iter = network->objects.begin();
            ::std::advance ( r_iter, index_1 );

            c_iter = r_iter->begin();
            ::std::advance ( c_iter, index_2 );

            // train the winning neuron
            (training_functional)
            (
                c_iter->weights,
                value,
                this->iteration
            );

            // increase iteration
            ++numeric_iterator;
            iteration = numeric_iterator();
        }
    };
    /*\@}*/

} // namespace neural_net

#endif // WTA_TRAINING_ALGORITM_HPP_INCLUDED

