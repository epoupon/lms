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
 * e-mail: habdank AT megapolis DOT pl
 * e-mail: janusz.rybarski AT gmail DOT com
 *
 * File created: Thu 08 Jun 2006 18:12:00 CEST
 * Last modified: Thu 08 Jun 2006 18:18:00 CEST
 */

#ifndef VALUE_TYPE_HPP_INCLUDED
#define VALUE_TYPE_HPP_INCLUDED

#include <boost/cstdint.hpp>

/**
 * \file value_type.hpp
 * \brief File contains template to recognize type of value.
 * \ingroup operators
 */

namespace operators
{
	/**
	 * \addtogroup operators
	 */
	/*\@{*/

	template < typename T >
	struct Value_type
	{
		typedef typename T::value_type type;
	};

	template <>
	struct Value_type < double >
	{
		typedef double type;
	};
	
	template <>
	struct Value_type < unsigned int >
	{
		typedef double type;
	};
	
    template <>
    struct Value_type < ::boost::int32_t >
	{
		typedef double type;
	};
	
	template <>
	struct Value_type < unsigned long >
	{
		typedef double type;
	};
	
	/*\@}*/

} // namespace operators

#endif // VALUE_TYPE_HPP_INCLUDED

