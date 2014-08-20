#ifndef METADATA_UTILS_HPP
#define METADATA_UTILS_HPP

#include <string>
#include <list>

#include <boost/locale.hpp>

namespace MetaData
{

bool readAsPosixTime(const std::string& str, boost::posix_time::ptime& time);

bool readList(const std::string& str, const std::string& separators, std::list<std::string>& results);

template<typename T>
static inline bool readAs(const std::string& str, T& data)
{
	std::istringstream iss ( str );
	return iss >> data;
}



std::string
static inline string_trim(const std::string& str,
		const std::string& whitespace = " \t")
{
	const auto strBegin = str.find_first_not_of(whitespace);
	if (strBegin == std::string::npos)
		return ""; // no content

	const auto strEnd = str.find_last_not_of(whitespace);
	const auto strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}


std::string
static inline string_to_utf8(const std::string& str)
{
	return boost::locale::conv::to_utf<char>(str, "UTF-8");
}

} // namespace MetaData

#endif

