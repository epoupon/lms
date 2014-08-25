#ifndef LOGGER_HPP__
#define LOGGER_HPP__

#include <map>

#include <boost/log/expressions/keyword_fwd.hpp>
#include <boost/log/expressions/keyword.hpp>

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>

#define LMS_LOG(module, level)	BOOST_LOG_SEV(Logger::instance().get(module), level)

enum Severity
{
	SEV_DEBUG	= 7,
	SEV_INFO	= 6,
	SEV_NOTICE	= 5,
	SEV_WARNING	= 4,
	SEV_ERROR	= 3,
	SEV_CRIT	= 2,
};

enum Module
{
	MOD_MAIN	= 0,
	MOD_UI,
	MOD_REMOTE,
};

BOOST_LOG_ATTRIBUTE_KEYWORD(module, "Module", Module)

class Logger
{
	public:

		static Logger& instance();

		struct Config {
			bool		enableFileLogging;
			bool		enableConsoleLogging;
			std::string	logPath;
			Severity	minSeverity;
		};

		//[ example_tutorial_file_advanced
		void init(const Config& config);

		boost::log::sources::severity_logger< Severity >&
			get(Module module) { return _loggers[module]; }

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
	static const char* const str[] =
	{
		"MAIN",
		"UI",
		"REMOTE",
	};
	if (static_cast< std::size_t >(val) < (sizeof(str) / sizeof(*str)))
		strm << str[val];
	else
		strm << static_cast< int >(val);
	return strm;
}


#endif // LOGGER_HPP__

