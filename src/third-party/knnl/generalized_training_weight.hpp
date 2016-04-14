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
 * File created: Sun 07 May 2006 13:51:04 CEST
 * Last modified: Wed 08 Aug 2007 18:21:59 CEST
 */

#ifndef GENERALIZED_TRAINING_WEIGHT_HPP_INCLUDED
#define GENERALIZED_TRAINING_WEIGHT_HPP_INCLUDED

#include "operators.hpp"
#include "training_functional.hpp"

/**
 * \file generalized_training_weight.hpp
 * \brief File contains template class Basic_generalized_training_weight and some other classes derived form that one.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Basic_generalized_training_weight
     * \brief Template class for the generalized training functions.
     * \param Value_type is a type of values.
     * \param Iteration_type is a type of interation counter.
     * \param Network_function_type is a type of function that will return proper value
     * based on network topology.
     * \param Space_funtion_type is a type of function that will return proper value
     * based on space topology.
     * \param Network_topology is a type of function that computes distances between
     * neurons based on network topology.
     * \param Space_topology is a type of function that computes distance between
     * value and weight in proper topology.
     * \param Index_type is a type of index in the neural network container.
     */
    template
    <
        typename Value_type,
        typename Iteration_type,
        typename Network_function_type,
        typename Space_function_type,
        typename Network_topology,
        typename Space_topology,
        typename Index_type
    >
    struct Basic_generalized_training_weight
    {
        typedef Network_function_type network_function_type;
        typedef Space_function_type space_function_type;
        typedef Value_type value_type;
        typedef Iteration_type iteration_type;
        typedef Index_type index_type;
        typedef Network_topology network_topology;
        typedef Space_topology space_topology;
    };

    /**
     * \class Classic_training_weight
     * \brief Template class for the generalize training functions that calculates
     * generalized weight classical way.
     * \param Value_type is a type of values.
     * \param Iteration_type is a type of interation counter.
     * \param Network_function_type is a type of function that will return proper value
     * based on network topology.
     * \param Space_funtion_type is a type of function that will return proper value
     * based on space topology.
     * \param Network_topology is a type of function that computes distances between
     * neurons based on network topology.
     * \param Space_topology is a type of function that computes distance between
     * value and weight in proper topology.
     * \param Index_type is a type of index in the neural network container.
     * \f[
     * y=n_f (n_t (c_1,c_2,v_1,v_2)) \cdot s_f (s_t (x,w))
     * \f]
     */
    template
    <
        typename Value_type,
        typename Iteration_type,
        typename Network_function_type,
        typename Space_function_type,
        typename Network_topology,
        typename Space_topology,
        typename Index_type
    >
    class Classic_training_weight
    : public Basic_generalized_training_weight
    <
        Value_type,
        Iteration_type,
        Network_function_type,
        Space_function_type,
        Network_topology,
        Space_topology,
        Index_type
    >
    {
    public:

        /** Functor computes weight based on the result from network topology. */
        Network_function_type network_function;

        /** Functor computes weight based on the result from space topology. */
        Space_function_type space_function;

        /** Functor computes generalized distance in network. */
        Network_topology network_topology;

        /** Functor computes generalized distance in data space. */
        Space_topology space_topology;

        /**
         * Function computes generalized weight for training proces.
         * This weight is not the same as weights in neural network.
         * \param weight is a weight from neural network.
         * \param value is a input value (value that trains network).
         * \param iteration is a number of training steps could be
         * number of training data sample.
         * \param c_1 is a row number (position) in the network of the central neuron.
         * \param c_2 is a column number (position) in the network of the central neuron.
         * \param v_1 is a row number (position) in the network of the trained neuron.
         * \param v_2 is a column number (position) in the network of the trained neuron.
         * \f[
         * y=n_f (n_t (c_1,c_2,v_1,v_2)) \cdot s_f (s_t (x,w))
         * \f]
         * where x is value and w is neuron weight.
         */
        typename Space_function_type::value_type operator()
        (
            Value_type const & weight,
            Value_type const & value,
            Iteration_type const & //iteration
            ,
            Index_type const & c_1,
            Index_type const & c_2,
            Index_type const & v_1,
            Index_type const & v_2
        )
        {
            // calculate result
            return
            (
                (network_function) ( (network_topology) ( c_1, c_2, v_1, v_2 ) )
                * (space_function) ( (space_topology) ( value, weight ) )
            );
        }

        /**
         * Constructor.
         * \param n_f is a network functor.
         * \param s_f is a data space functor.
         * \param n_t is a network topology functor.
         * \param s_t is a space topology functor.
         */
        Classic_training_weight
        (
            Network_function_type const & n_f,
            Space_function_type const & s_f,
            Network_topology const & n_t,
            Space_topology const & s_t
        )
        : Basic_generalized_training_weight
        <
            Value_type,
            Iteration_type,
            Network_function_type,
            Space_function_type,
            Network_topology,
            Space_topology,
            Index_type
        >(),
        network_function ( n_f ),
        space_function ( s_f ),
        network_topology ( n_t ),
        space_topology ( s_t )
        {}

        /** Copy constructor */
        template
        <
            typename Value_type_2,
            typename Iteration_type_2,
            typename Network_function_type_2,
            typename Space_function_type_2,
            typename Network_topology_2,
            typename Space_topology_2,
            typename Index_type_2
        >
        Classic_training_weight
        (
            const Classic_training_weight
            <
                Value_type_2,
                Iteration_type_2,
                Network_function_type_2,
                Space_function_type_2,
                Network_topology_2,
                Space_topology_2,
                Index_type_2
            >
            & classic_training_weight
        )
        : Basic_generalized_training_weight
        <
             Value_type_2,
             Iteration_type_2,
             Network_function_type_2,
             Space_function_type_2,
             Network_topology_2,
             Space_topology_2,
             Index_type_2
        >(),
        network_function ( classic_training_weight.network_function ),
        space_function ( classic_training_weight.space_function ),
        network_topology ( classic_training_weight.network_topology ),
        space_topology ( classic_training_weight.space_topology )
        {}

    };

    /**
     * \class Experimental_training_weight
     * \brief Template class for the generalize training functions that calculates
     * generalizeg weight in experimental way.
     * \param Value_type is a type of values.
     * \param Iteration_type is a type of interation counter.
     * \param Network_function_type is a type of function that will return proper value
     * based on network topology.
     * \param Space_funtion_type is a type of function that will return proper value
     * based on space topology.
     * \param Network_topology is a type of function that computes distances between
     * neurons based on network topology.
     * \param Space_topology is a type of function that computes distance between
     * value and weight in proper topology.
     * \param Index_type is a type of index in the neural network container.
     * \param Parameters_type is a type of the parameters for experimenntal training.
     * \param n_power is a power q_N
     * \param s_power is a power q_S
     * \f[
     * y= ( p_1 \cdot n_f (n_t (c_1,c_2,v_1,v_2)) - p_0 )^q_N \cdot s_f (s_t (x,w))^q_S
     * \f]
     */
    template
    <
        typename Value_type,
        typename Iteration_type,
        typename Network_function_type,
        typename Space_function_type,
        typename Network_topology,
        typename Space_topology,
        typename Index_type,
        typename Parameter_type,
        ::boost::int32_t n_power = 1,
        ::boost::int32_t s_power = 1
    >
    class Experimental_training_weight
    : public Basic_generalized_training_weight
    <
        Value_type,
        Iteration_type,
        Network_function_type,
        Space_function_type,
        Network_topology,
        Space_topology,
        Index_type
    >
    {
    public:

        //::boost::int32_t const s_power;
        //::boost::int32_t const n_power;

        /** Scaling parameter. */
        Parameter_type parameter_1;

        /** Shifting parameter. */
        Parameter_type parameter_0;


        /** Functor computes weight based on the result from network topology. */
        Network_function_type network_function;

        /** Functor computes weight based on the result from space topology. */
        Space_function_type space_function;

        /** Functor computes generalized distance in network. */
        Network_topology network_topology;

        /** Functor computes generalized distance in data space. */
        Space_topology space_topology;

        /**
         * Constructor.
         * \param n_f is a network functor.
         * \param s_f is a data space functor.
         * \param n_t is a network topology functor.
         * \param s_t is a space topology functor.
         * \param parameter_0_ is a scailing parameter.
         * \param parameter_1_ is a shifting parameter.
         */
        Experimental_training_weight
        (
            Network_function_type const & n_f,
            Space_function_type const & s_f,
            Network_topology const & n_t,
            Space_topology const & s_t,
            Parameter_type const & parameter_0_,
            Parameter_type const & parameter_1_//,
//            ::boost::int32_t const s_power_ = 1,
//            ::boost::int32_t const n_power_ = 1
        )
        : //s_power ( s_power_ ),
        //n_power ( n_power_ ),
        parameter_1 ( parameter_1_),
        parameter_0 ( parameter_0_),
        network_function ( n_f ),
        space_function ( s_f ),
        network_topology ( n_t ),
        space_topology ( s_t )
        {}

        /** Copy constructor. */
        template
        <
            typename Value_type_2,
            typename Iteration_type_2,
            typename Network_function_type_2,
            typename Space_function_type_2,
            typename Network_topology_2,
            typename Space_topology_2,
            typename Index_type_2,
            typename Parameter_type_2
        >
        Experimental_training_weight
        (
            const Experimental_training_weight
            <
                Value_type_2,
                Iteration_type_2,
                Network_function_type_2,
                Space_function_type_2,
                Network_topology_2,
                Space_topology_2,
                Index_type_2,
                Parameter_type_2
            >
            & experimental_training_weight
        )
        : Basic_generalized_training_weight
        <
            Value_type_2,
            Iteration_type_2,
            Network_function_type_2,
            Space_function_type_2,
            Network_topology_2,
            Space_topology_2,
            Index_type_2
        >(),
        parameter_1 ( experimental_training_weight.parameter_1_),
        parameter_0 ( experimental_training_weight.parameter_0_),
        network_function ( experimental_training_weight.n_f ),
        space_function ( experimental_training_weight.s_f ),
        network_topology ( experimental_training_weight.n_t ),
        space_topology ( experimental_training_weight.s_t )
        {}

        /**
         * Function computes generalized weight for training proces.
         * This weight is not the same as weights in neural network.
         * \param weight is a weight from neural network.
         * \param value is a input value (value that trains network).
         * \param iteration is a number of training steps could be
         * number of training data sample.
         * \param c_1 is a row number (position) in the network of the central neuron.
         * \param c_2 is a column number (position) in the network of the central neuron.
         * \param v_1 is a row number (position) in the network of the trained neuron.
         * \param v_2 is a column number (position) in the network of the trained neuron.
         * \f[
         * y= ( p_1 \cdot n_f (n_t (c_1,c_2,v_1,v_2)) - p_0 ) \cdot s_f (s_t (x,w))
         * \f]
         * where x is value, w is neuron weight, \f$p_1\f$ is scaling parameter, \f$p_0\f$ is shifting parameter.
         */
        typename Space_function_type::value_type operator()
        (
            Value_type & weight,
            Value_type const & value,
            Iteration_type const & //iteration
            ,
            Index_type const & c_1,
            Index_type const & c_2,
            Index_type const & v_1,
            Index_type const & v_2
        ) const
        {
            //::operators::power < typename Space_function_type::value_type, ::boost::int32_t > power_v;
            // calculate result
            return
            (
            //    power_v( parameter_1 * (network_function) ( (network_topology) ( c_1, c_2, v_1, v_2 ) ) - parameter_0, n_power )
            //    * power_v( (space_function) ( (space_topology) ( value, weight ) ), s_power )
                ::operators::static_power<typename Space_function_type::value_type,n_power>( parameter_1 * (network_function) ( (network_topology) ( c_1, c_2, v_1, v_2 ) ) - parameter_0 )
                * ::operators::static_power<typename Space_function_type::value_type,s_power>( (space_function) ( (space_topology) ( value, weight ) ) )
            );
        }
    };
    /*\@}*/

} // namespace neural_net

#endif // GENERALIZED_TRAINING_WEIGHT_HPP_INCLUDED

