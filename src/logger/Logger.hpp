/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOGGER_HPP__
#define LOGGER_HPP__

#include <map>

#include <boost/log/expressions/keyword_fwd.hpp>
#include <boost/log/expressions/keyword.hpp>

#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>

#define LMS_LOG(module, level)	BOOST_LOG_SEV(Logger::instance().get(module), level)

enum Severity
{
	SEV_CRIT	= 2,
	SEV_ERROR	= 3,
	SEV_WARNING	= 4,
	SEV_NOTICE	= 5,
	SEV_INFO	= 6,
	SEV_DEBUG	= 7,
};

enum Module
{
	MOD_AV		= 0,
	MOD_COVER,
	MOD_DB,
	MOD_DBUPDATER,
	MOD_MAIN,
	MOD_METADATA,
	MOD_REMOTE,
	MOD_SERVICE,
	MOD_TRANSCODE,
	MOD_UI,
};

BOOST_LOG_ATTRIBUTE_KEYWORD(module, "Module", Module)

class Logger
{
	public:

		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;

		static Logger& instance();

		void init();

		boost::log::sources::severity_logger< Severity >&
			get(Module module);

	private:
		Logger();

		std::map<Module, boost::log::sources::severity_logger< Severity > > _loggers;
};

// The formatting logic for the severity level
template< typename CharT, typename TraitsT >
inline std::basic_ostream< CharT, TraitsT >& operator<< (
    std::basic_ostream< CharT, TraitsT >& strm, Severity lvl)
{
	static const char* const str[] =
	{
		"",
		"",
		"CRIT",
		"ERROR",
		"WARNING",
		"NOTICE",
		"INFO",
		"DEBUG"
	};
	if (static_cast< std::size_t >(lvl) < (sizeof(str) / sizeof(*str)))
		strm << str[lvl];
	else
		strm << static_cast< int >(lvl);
	return strm;
}

template< typename CharT, typename TraitsT >
inline std::basic_ostream< CharT, TraitsT >& operator<< (
    std::basic_ostream< CharT, TraitsT >& strm, Module val)
{
	const char* res = NULL;

	switch(val)
	{
		case 	MOD_AV: 	res = "AV"; break;
		case 	MOD_COVER: 	res = "COVER"; break;
		case 	MOD_DB: 	res = "DB"; break;
		case 	MOD_DBUPDATER: 	res = "DBUPDATER"; break;
		case 	MOD_MAIN: 	res = "MAIN"; break;
		case 	MOD_METADATA: 	res = "METADATA"; break;
		case 	MOD_REMOTE: 	res = "REMOTE"; break;
		case 	MOD_SERVICE: 	res = "SERVICE"; break;
		case 	MOD_TRANSCODE: 	res = "TRANSCODE"; break;
		case 	MOD_UI:		res = "UI"; break;
	}

	if (res)
		strm << res;
	else
		strm << static_cast< int >(val);
	return strm;
}


#endif // LOGGER_HPP__

