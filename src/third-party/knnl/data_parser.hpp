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
 * File created: Sat 29 Apr 2006 19:10:50 CEST
 * Last modified: Wed 08 Aug 2007 17:15:45 CEST
 */

#ifndef DATA_PARSER_HPP_INCLUDED
#define DATA_PARSER_HPP_INCLUDED

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

/**
 * \defgroup data_parser Data parser
 */

/**
 * \file data_parser.hpp
 * \brief File contains template class Data_parser.
 * \ingroup data_parser
 */

/**
 * \addtogroup data_parser
 */
/*\@{*/

/**
 * \namespace data_parser
 * \brief Data parser namespace includes functionality to parsing and storing data into containers.
 * \ingroup data_parser
 */
namespace data_parser
{
    /**
     * Class for parsing data stream.
     * \param Data_container is a container type.
     */
    template < typename Data_container >
    class Data_parser
    {
    public:
        /** Constructor. */
        Data_parser()
        {}

        /**
         * Function that parse stream and store data in container.
         * \param is is a reference to the stream.
         * \param data_container is a reference to the container.
         * \return modified container.
         * \throw ::std::runtime_error when after parsing size of data_container is still zero.
         */
        Data_container & operator() ( ::std::istream & is, Data_container & data_container ) const
        {
            ::std::string tmp_string;
            ::boost::int32_t counter = 0;

            typename Data_container::value_type tmp_sub_container;

            // get line of data from stream
            while ( ::std::getline ( is, tmp_string ) )
            {
                // count data
                ++counter;

                // parse data and push data back into the container
                data_container.push_back ( parse_string ( tmp_sub_container, tmp_string ) );

                // reset temporal container
                tmp_sub_container.clear();
            }

            // if data container is empty after all procedure throw exception
            if ( data_container.size() == 0 )
            {
                throw ::std::runtime_error ( "Data file corupted." );
            }

            // return data container of containers
            return data_container;
        }

    private:
        /**
         * This function is for parsing string.
         * \param container is a reference to the data conatiner.
         * \param str is a reference to the string.
         * \return container of the values stored in one string.
         */
        typename Data_container::value_type parse_string
        (
            typename Data_container::value_type & container,
            ::std::string & str
        ) const
        {
            ::std::stringstream tmp_sstr ( str );

            typedef typename Data_container::value_type::value_type internal_type;

            // parse data as sequence of fixed type data and store them into the container.
            ::std::copy
            (
                ::std::istream_iterator < internal_type > ( tmp_sstr ),
                ::std::istream_iterator < internal_type >(),
                ::std::back_inserter ( container )
            );

            return container;
        }

    };
} // namespace data_parser
/*\@}*/

#endif // DATA_PARSER_HPP_INCLUDED
