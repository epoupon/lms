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
 * File created: Fri 14 Apr 2006 22:37:57 CEST
 * Last modified: Wed 08 Aug 2007 18:29:17 CEST
 */

#ifndef RECTANGULAR_CONTAINER_HPP_INCLUDED
#define RECTANGULAR_CONTAINER_HPP_INCLUDED

#include <vector>

/**
 * \file rectangular_container.hpp
 * \brief File contains class Rectangular_container
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * Rectangular_grid_container template class.
     * \param Object_type is type of stored objects.
     * \todo TODO: Rectangular_container class should be rewritten
     * using new version of ::boost::matrix.
     */
    template
    <
        typename Object_type
    >
    class Rectangular_container
    {
    public:

        typedef Object_type value_type;
        typedef typename ::std::vector < Object_type > column_type;
        typedef typename ::std::vector < column_type > row_type;

        typedef typename row_type::iterator row_iterator;
        typedef typename column_type::iterator column_iterator;

        typedef typename row_type::size_type row_size_t;
        typedef typename column_type::size_type col_size_t;

        /**
        * Stores size of container.
        */
        typedef ::std::pair < row_size_t, col_size_t > Matrix_index;

        /** Objects. */
        row_type objects;

        /**
         * Get object.
         * \param i is the row number.
         * \param j is the column number.
         * \return const reference to the (i,j)-th object.
         */
        Object_type const & operator() ( row_size_t i, col_size_t j )
        {
            return objects[i][j];
        }

        /**
         * Constructor.
         */
        Rectangular_container()
        {}

        /** Copy constructor. */
        template < typename Object_type_2 >
        Rectangular_container
        (
            Rectangular_container < Object_type_2 > const & rectangular_container
        )
        : objects ( rectangular_container.objects )
        {}

        inline col_size_t get_no_columns() const
        {
            return objects.begin()->size();
        }

        inline row_size_t get_no_rows() const
        {
            return objects.size();
        }

        inline Matrix_index get_size() const
        {
            Matrix_index idx;
            idx.first = objects.size();
            idx.second = objects.begin()->size();

            return idx;
        }

    };
    /*\@}*/

} // namespace neural_net

#endif // RECTANGULAR_CONTAINER_HPP_INCLUDED

