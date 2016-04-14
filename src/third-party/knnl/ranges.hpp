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
 * File created: Mon 24 Apr 2006 12:11:38 CEST
 * Last modified: Wed 08 Aug 2007 17:27:38 CEST
 */

#ifndef RANGES_HPP_INCLUDED
#define RANGES_HPP_INCLUDED

#include <utility>
#include <iostream>

/**
 * \file ranges.hpp
 * \brief File contains template class Ranges.
 * \ingroup neural_net
 */

namespace neural_net
{
    /**
    * \addtogroup neural_net
    */
    /*\@{*/

    /**
     * \class Ranges
     * \brief Class creates and claculates ranges of data.
     * \param Container_type is a type of container.
     */
    template < typename Container_type >
    class Ranges
    {
    public:
        /**
         * Constructor.
         * \param value_ is a value that will be set at he begining.
         */
        explicit Ranges ( typename Container_type::value_type const & value_ )
        :ranges ( value_, value_ )
        {}

        /** Copy constructor. */
        template < typename Container_type_2 >
        Ranges ( Ranges < Container_type_2 > const & ranges_ )
        : ranges ( ranges_.ranges )
        {}

        /**
         * Function that is going through the all container.
         * \param value is a reference to the container.
         */
        void operator() ( Container_type const & value )
        {
            typename Container_type::const_iterator pos;

            typename Container_type::value_type::const_iterator pos_range;
            typename Container_type::value_type::iterator pos_min_range;
            typename Container_type::value_type::iterator pos_max_range;

            // go through the all data in container and ...
            for ( pos = value.begin(); pos != value.end(); ++pos )
            {
                // look for the highest and lowest values to set ranges.
                // iterate through position of the container and ranges type to compare
                // proper values and set ranges.
                for
                (
                    pos_min_range = ranges.first.begin(),
                    pos_max_range = ranges.second.begin(),
                    pos_range = pos->begin();
                    pos_min_range != ranges.first.end();
                    ++pos_min_range, ++pos_max_range, ++pos_range
                )
                {
                    *pos_min_range = ::std::min ( *pos_min_range, *pos_range );
                    *pos_max_range = ::std::max ( *pos_max_range, *pos_range );
                }
            }
        }

        /**
         * Function returns supremum value of the data in the container.
         * This value could not exests in container, because is created from
         * all maximums of data e.g. { (1,2), (2,1) } -> (2,2)
         */
        typename Container_type::value_type const & get_max() const
        {
            return ranges.second;
        }

        /**
         * Function returns infimum of the data in the container.
         * This value could not exests in container, because is created from
         * all minimums of data e.g. { (1,2), (2,1) } -> (1,1)
         */
        typename Container_type::value_type const & get_min() const
        {
            return ranges.first;
        }

    protected:
        Ranges();

    private:
        ::std::pair
        <
            typename Container_type::value_type,
            typename Container_type::value_type
        > ranges;

    };
    /*\@}*/

} // namespace neural_net

#endif // RANGES_HPP_INCLUDED

