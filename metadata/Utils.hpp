#ifndef METADATA_UTILS_HPP
#define METADATA_UTILS_HPP

#include <string>
#include <list>

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

}

#endif

