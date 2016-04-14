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
 * File created: Wed 03 May 2006 13:18:41 CEST
 * Last modified: Wed 08 Aug 2007 18:22:48 CEST
 */

#ifndef WTM_TOPOLOGY_HPP_INCLUDED
#define WTM_TOPOLOGY_HPP_INCLUDED

#include <cmath>

#include "operators.hpp"

/**
 * \file wtm_topology.hpp
 * \brief File contains template classes for setting up topology in the network.
 * \ingroup neural_net
 */
namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Basic_topology
     * \brief Basic class for topologies.
     * \param Result_type type of result of the topology as a functor
     * that calculates didtance between two points
     * in particular case two neurons.
     * \param Index_type is a type is index that is used in neural network.
     */
    template
    <
        typename Result_type,
        typename Index_type
    >
    class Basic_topology
    {
    public:
        typedef Result_type result_type;
        typedef Index_type value_type;
    };

    /**
     * \class City_topology
     * \brief Topology for neural network that calculates distance between two neurons.
     * \param Index_type is a type of index.
     * \f[
     * d (n,m) = |n_1-m_1| + |n_2-m_2|
     * \f]
     */
    template < typename Index_type >
    class City_topology
    : public Basic_topology < Index_type, Index_type >
    {
    public:

        /**
         * Function claculates distance.
         * \param index_1_1 is first index of first neuron.
         * \param index_1_2 is second index of first neuron.
         * \param index_2_1 is first index of second neuron.
         * \param index_2_2 is second index of second neuron.
         * \f[
         * d (n,m) = |n_1-m_1| + |n_2-m_2|
         * \f]
         * where: n is first neuron, m is second neuron.
         */
        inline Index_type operator()
        (
            Index_type const & index_1_1,
            Index_type const & index_1_2,
            Index_type const & index_2_1,
            Index_type const & index_2_2
        ) const
        {
            return
            (
                ::operators::abs ( index_1_1 - index_2_1 )
                + ::operators::abs ( index_1_2 - index_2_2 )
            );
        }
    };

    /**
     * \class Max_topology
     * \brief Topology for neural network that calculates distance between two neurons.
     * \param Index_type is a type of index.
     * \f[
     * d (n,m) = \max ( |n_1-m_1|, |n_2-m_2| )
     * \f]
     */
    template < typename Index_type >
    class Max_topology
    : public Basic_topology < Index_type, Index_type >
    {
    public:

        /**
         * Function claculates distance.
         * \param index_1_1 is first index of first neuron.
         * \param index_1_2 is second index of first neuron.
         * \param index_2_1 is first index of second neuron.
         * \param index_2_2 is second index of second neuron.
         * \f[
         * d (n,m) = \max ( |n_1-m_1|, |n_2-m_2| )
         * \f]
         * where: n is first neuron, m is second neuron.
         */
        inline Index_type operator()
        (
            Index_type const & index_1_1,
            Index_type const & index_1_2,
            Index_type const & index_2_1,
            Index_type const & index_2_2
        ) const
        {
            return
            (
                ::std::max
                (
                    ::operators::abs ( index_1_1 - index_2_1 ),
                    ::operators::abs ( index_1_2 - index_2_2 )
                )
            );
        }
    };

    /**
     * \class Hexagonal_topology
     * \brief Topology for neural network that calculates distance between two neurons.
     * \param Index_type is a type of index.
     * \f{eqnarray*}
     * h_1 (x) & = & \frac{ x_1 + 1 }{2} + x_2\\
     * h_2 (x) & = & \frac{h_{off}}{2} + x_2 - x_1 / 2;\\
     * t_1 & = & \max ( h_1 (n), h_1 (m) ) - \min ( h_1 (n), h_1 (m) )\\
     * t_2 & = & \max ( h_2 (n), h_2 (m) ) - \min ( h_2 (n), h_2 (m) )\\
     * d (n,m) & = & \left\{
     * \begin{array}{ll}
     * \max (|t_1|,|t_2|) & if \; sign \; of \; t_1 \; and \; t_2 \; is \; the \; same\\
     * |t_1|+|t_2| & otherwise
     * \end{array} \right.
     * \f}
     */
    template < typename Index_type >
    class Hexagonal_topology
    : public Basic_topology < Index_type, Index_type >
    {
    public:
        /**
         * Constructor.
         * \param hex_offset_ is an offset of the hexagonal topology.
         * This value have to be not less than number of rows in neuron
         * container counted from 0.
         */
        inline explicit Hexagonal_topology ( Index_type const & hex_offset_ )
        : hex_offset ( hex_offset_ )
        {}

        /** Copy constructor. */
        template < typename Index_type_2 >
        inline Hexagonal_topology ( Hexagonal_topology < Index_type_2 > const & hex_topology )
        : Basic_topology < Index_type_2, Index_type_2 >(),
        hex_offset ( hex_topology.hex_offset )
        {}

        /**
         * Function calculates distance.
         * \param index_1_1 is first index of first neuron.
         * \param index_1_2 is second index of first neuron.
         * \param index_2_1 is first index of second neuron.
         * \param index_2_2 is second index of second neuron.
         * \f{eqnarray*}
         * h_1 (x) & = & \frac{ x_1 + 1 }{2} + x_2\\
         * h_2 (x) & = & \frac{h_{off}}{2} + x_2 - x_1 / 2;\\
         * t_1 & = & \max ( h_1 (n), h_1 (m) ) - \min ( h_1 (n), h_1 (m) )\\
         * t_2 & = & \max ( h_2 (n), h_2 (m) ) - \min ( h_2 (n), h_2 (m) )\\
         * d (n,m) & = & \left\{
         * \begin{array}{ll}
         * \max (|t_1|,|t_2|) & if \; sign \; of \; t_1 \; and \; t_2 \; is \; the \; same\\
         * |t_1|+|t_2| & otherwise
         * \end{array} \right.
         * \f}
         * where: n is firs neuron, m is second.
         */
        Index_type operator()
        (
            Index_type const & index_1_1,
            Index_type const & index_1_2,
            Index_type const & index_2_1,
            Index_type const & index_2_2
        ) const
        {
            Index_type hex_index_1_1;
            Index_type hex_index_1_2;
            Index_type hex_index_2_1;
            Index_type hex_index_2_2;

            Index_type tmp_hex_index_1;
            Index_type tmp_hex_index_2;

            // recalculate indexes to the better indexes used in hexagonal space
            hex_index_1_1 = ( index_1_1 + 1 ) / 2 + index_1_2;
            hex_index_1_2 = ( hex_offset / 2 + index_1_2 ) - index_1_1 / 2;

            hex_index_2_1 = ( index_2_1 + 1 ) / 2 + index_2_2;
            hex_index_2_2 = ( hex_offset / 2 + index_2_2 ) - index_2_1 / 2;

            // calculate difference between points in hexagonal space
            tmp_hex_index_1 = ::std::max ( hex_index_1_1, hex_index_2_1 )
                                - ::std::min ( hex_index_1_1, hex_index_2_1 );
            tmp_hex_index_2 = ::std::max ( hex_index_1_2, hex_index_2_2 )
                                - ::std::min ( hex_index_1_2, hex_index_2_2 );

            // here we have special algebra to calculate distance,
            // bacause of special basis in this space:
            // ( 1, 1 ); ( -1, -1 ) have distance 1 the same as
            // ( -1, 0 ); ( 0, -1 ); ( 1, 0 ); ( 0, 1 ).
            if ( tmp_hex_index_1 == 0 && tmp_hex_index_2 == 0 )
            {
                return static_cast < Index_type > ( 0 );
            }

            // check if values have the same direction if yes it means that we have to use
            // reasoning based on assumption that ( -1, -1 ) and ( 1, 1 ) have distance 1.
            if ( ( ( hex_index_1_1 > hex_index_2_1 ) && ( hex_index_1_2 > hex_index_2_2 ) )
                 || ( ( hex_index_1_1 < hex_index_2_1 ) && ( hex_index_1_2 < hex_index_2_2 ) )
            )
            {
                return ::std::max ( tmp_hex_index_1, tmp_hex_index_2 );
            }
            else
            {
                return ::operators::abs ( tmp_hex_index_1 ) + ::operators::abs ( tmp_hex_index_2 );
            }

            return static_cast < Index_type > ( 0 );
        }

    protected:
        Index_type hex_offset;
    };
    /*\@}*/

} // namespace neural_net

#endif // WTM_TOPOLOGY_HPP_INCLUDED

