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
 * File created: Wed 26 Apr 2006 14:55:28 CEST
 * Last modified: Wed 08 Aug 2007 18:25:18 CEST
 */

#ifndef TRAINING_FUNCTIONAL_HPP_INCLUDED
#define TRAINING_FUNCTIONAL_HPP_INCLUDED

/**
 * \file training_functional.hpp
 * \brief File contains template classes that support
 * training proces of the network.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Basic_training_functional
     * \brief Basic trainng functional.
     */
    class Basic_training_functional
    {};

    /**
     * \class Basic_wta_training_functional
     * \brief Class that is basic for Winner Takes All (WTA) algorithms.
     * \param Value_type is a type of values.
     * \param Parameters_type is a type of parameters of training functional.
     */
    template
    <
        typename Value_type,
        typename Parametres_type
    >
    class Basic_wta_training_functional
    : public Basic_training_functional
    {
    public:
        typedef Value_type value_type;
        typedef Parametres_type parameters_type;
    };

    /**
     * \class Basic_wtm_training_functional
     * \brief Class that is basic for Winner Takes Most (WTM) algorithms.
     * \param Value_type is a type of values.
     * \param Parameters_type is a type of parameters of training functional.
     * \param Iteration_type is a type of step counter value.
     * \param Index_type is a type of index used in network.
     * \param Topology_type is a type of topology that should be used
     * for measuring distance in network.
     */
    template
    <
        typename Value_type,
        typename Parameters_type,
        typename Iteration_type,
        typename Index_type,
        typename Topology_type
    >
    class Basic_wtm_training_functional
        : public Basic_wta_training_functional
        < Value_type, Parameters_type >
    {
    public:
        typedef Iteration_type iteration_type;
        typedef Index_type index_type;
        typedef Topology_type topology_type;
    };

    /**
     * \class Wta_proportional_training_functional
     * \brief Class that certain kind of WTA algorithm.
     * \param Value_type is a type of values.
     * \param Parameters_type is a type of parameters of training functional.
     * \param Iteration_type is a type of step counter value.
     * Algorithm in the time of training for single data set could change
     * weight in training function.
     * \f[
     * w_{i,j} (t+1)=w_{i,j} (t) + ( p_0 + p_1 * s ) * ( x (t) - w_{i,j} (t) )
     * \f]
     */
    template
    <
        typename Value_type,
        typename Parameters_type,
        typename Iteration_type
    >
    class Wta_proportional_training_functional
    : public Basic_wta_training_functional
    <
        Value_type,
        Parameters_type
    >
    {
    public:

        typedef Iteration_type iteration_type;

        /** Shifting parameter for linear function */
        Parameters_type parameter_0;

        /** Scaling parameter for linear function */
        Parameters_type parameter_1;

        /**
         * Constructor.
         * \param parameter_0_ is a shifting parameter for linear function
         * used for training.
         * \param parameter_1_ is a scaling parameter for linear function
         * used for training.
         */
        Wta_proportional_training_functional
        (
            Parameters_type const & parameter_0_,
            Parameters_type const & parameter_1_
        )
        : parameter_0 ( parameter_0_ ), parameter_1 ( parameter_1_ )
        {}

        /** Copy constructor. */
        template
        <
            typename Value_type_2,
            typename Parameters_type_2,
            typename Iteration_type_2
        >
        Wta_proportional_training_functional
        (
            Wta_proportional_training_functional
            <
                Value_type_2,
                Parameters_type_2,
                Iteration_type_2
            >
            const & training_functional_
        )
        : Basic_wta_training_functional < Value_type, Parameters_type >(),
        parameter_0 ( training_functional_.parameter_0 ),
        parameter_1 ( training_functional_.parameter_1 )
        {}

        /**
         * Training function.
         * \param weight is weight of the neuron.
         * \param value is value that trains winner neuron.
         * \param s is step number.
         * \return modified weight.
         * \f[
         * w_{i,j} (t+1)=w_{i,j} (t) + ( p_0 + p_1 * s ) * ( x (t) - w_{i,j} (t) )
         * \f]
         * where: x is value, w is weight and s is step number.
         */
        Value_type & operator()
        (
            Value_type & weight,
            Value_type const & value,
            iteration_type const & s
        ) const
        {
            using namespace ::operators;
            return
            (
                weight
                    = weight
                    + ( parameter_0 + parameter_1 * s )
                    * ( value - weight )
            );
        }
    };

    /**
     * \class Wtm_classical_training_functional
     * \brief Class that is basic for Winner Takes Most (WTM) algorithms.
     * \param Value_type is a type of values.
     * \param Parameters_type is a type of parameters of training functional.
     * \param Iteration_type is a type of step counter value.
     * \param Index_type is a type of index used in network.
     * \param Generalized_training_weight_type is a type of functor that will
     * be used for set up training weight.
     */
    template
    <
        typename Value_type,
        typename Parameters_type,
        typename Iteration_type,
        typename Index_type,
        typename Generalized_training_weight_type
    >
    class Wtm_classical_training_functional
    : public Basic_wtm_training_functional
    <
        Value_type,
        Parameters_type,
        Iteration_type,
        Index_type,
        Generalized_training_weight_type
    >
    {
    public:

        /** Final scaling of the function. */
        Parameters_type parameter;

        /** Functor calculates training weight. */
        Generalized_training_weight_type generalized_training_weight;

        /**
         * Constructor.
         * \param generalized_weight is a reference to the functor.
         * \param parameter_ is a reference to scaling parameter.
         */
        Wtm_classical_training_functional
        (
            Generalized_training_weight_type const & generalized_weight,
            Parameters_type const & parameter_
        )
        : Basic_wtm_training_functional
        <
            Value_type,
            Parameters_type,
            Iteration_type,
            Index_type,
            Generalized_training_weight_type
        >(),
        parameter ( parameter_ ),
        generalized_training_weight ( generalized_weight )
        {}

        /** Copy constructor. */
        template
        <
            typename Value_type_2,
            typename Parameters_type_2,
            typename Iteration_type_2,
            typename Index_type_2,
            typename Generalized_training_weight_type_2
        >
        Wtm_classical_training_functional
        (
            const Wtm_classical_training_functional
            <
                Value_type_2,
                Parameters_type_2,
                Iteration_type_2,
                Index_type_2,
                Generalized_training_weight_type_2
            >
            & training_functional_
        )
        : Basic_wtm_training_functional
        <
            Value_type,
            Parameters_type,
            Iteration_type,
            Index_type,
            Generalized_training_weight_type
        >(),
        parameter ( training_functional_.parameter ),
        generalized_training_weight ( training_functional_.generalized_training_weight )
        {}

        /**
         * Function calculates new value of the neuron weight.
         * \param weight is a reference to the trained neuron weight.
         * \param value is a reference to the value that trains neuron.
         * \param s is step number.
         * \param center_i is a first index of the central neuron (row number).
         * \param center_j is a second index of the central neuron (column number).
         * \param i_ is a first index of a trained neuron (row number).
         * \param j_ is a second index of a trained neuron (column number).
         * \return modified neuron weight.
         * \f[
         * w_{i,j} (t+1) = w_{i,j} (t) + G ( w_{i,j} (t), x (t), s, c_i, c_j, i, j ) * ( x (t) - w_{i,j} (t) )
         * \f]
         */
        Value_type & operator()
        (
            Value_type & weight,
            Value_type const & value,
            Iteration_type const & s,
            Index_type const & center_i,
            Index_type const & center_j,
            Index_type const & i_,
            Index_type const & j_
        )
        {
            using namespace ::operators;

            return
            (
                weight
                    = weight
                        + parameter
                        * (generalized_training_weight)
                        (
                            weight,
                            value,
                            s,
                            center_i, center_j,
                            i_, j_
                        )
                        * ( value - weight )
            );
        }
    };
    /*\@}*/

} // namespace neural_net

#endif // TRAINING_FUNCTIONAL_HPP_INCLUDED

