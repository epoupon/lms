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
 * File created: Tue 18 Apr 2006 19:25:15 CEST
 * Last modified: Wed 08 Aug 2007 17:23:33 CEST
 */

#ifndef KOHONEN_NETWORK_HPP_INCLUDED
#define KOHONEN_NETWORK_HPP_INCLUDED

#include "ranges.hpp"

#include <cstdlib>
#include <vector>
#include <ctime>

/**
 * \file kohonen_network.hpp
 * \brief File contains template functions for preparing
 * Kohonen neural networks.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * Function generates randomly distributed weights for neural network.
     * Distribution is uniform and weights are generated based on
     * multidimensional ranges of training data.
     * \param no_rows is a number of rows that will be created in neural network.
     * \param no_columns is a number of columns that will be created in neural network.
     * \param activation_function is activation function that will be set.
     * \param binary_operation is a function tat will be set under activation function.
     * \param data is a reference to data container.
     * \param kohonen_network is a reference to the network.
     * \param randomize_policy is a policy class for setting up random number generator.
     * \todo TODO: When Rectangular_container will be changed then this class should be repaired too.
     */
    template
    <
        typename Data_container_type,
        typename Kohonen_network_type,
        typename Randomize_policy
    >
    void generate_kohonen_network
    (
        typename Kohonen_network_type::row_type::size_type const & no_rows,
        typename Kohonen_network_type::column_type::size_type const & no_columns,
        typename Kohonen_network_type::value_type::activation_function_type const & activation_function,
        typename Kohonen_network_type::value_type::binary_operation_type const & binary_operation,
        Data_container_type & data,
        Kohonen_network_type & kohonen_network,
        Randomize_policy const & randomize_policy
    )
    {
        randomize_policy();

        typedef typename Kohonen_network_type::value_type Neuron_type;
        typedef typename Kohonen_network_type::row_type::size_type row_size_t;
        typedef typename Kohonen_network_type::column_type::size_type col_size_t;
        typedef typename Neuron_type::weights_type weights_t;
        typedef typename weights_t::size_type w_size_t;

        ::std::vector < Neuron_type > tmp_neuron_vector;
        tmp_neuron_vector.reserve ( no_columns );

        const w_size_t K = data.begin()->size();
        weights_t weights ( K );

        Ranges < Data_container_type > data_ranges ( *data.begin() );
        data_ranges ( data );

        for ( row_size_t i = 0; i < no_rows; ++i )
        {
            for ( col_size_t j = 0; j < no_columns; ++j )
            {
                for ( w_size_t k = 0; k < K; ++k )
                {
                    weights[k] = (
                        static_cast < typename Data_container_type::value_type::value_type >
                        ( ( data_ranges.get_max().at ( k )
                            - data_ranges.get_min().at ( k ) )
                         * ( rand() / ( 1.0 + RAND_MAX ) )
                         + data_ranges.get_min().at ( k ) ) );
                }

                Neuron_type local_neuron
                (
                    weights,
                    activation_function,
                    binary_operation
                );
                tmp_neuron_vector.push_back ( local_neuron );
            }
            kohonen_network.objects.push_back ( tmp_neuron_vector );
            tmp_neuron_vector.clear();
        }
    }
    /*\@}*/

} // namespace neural_net

#endif // KOHONEN_NETWORK_HPP_INCLUDED

