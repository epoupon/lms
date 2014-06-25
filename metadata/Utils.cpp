#include <string>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>

#include "Utils.hpp"

namespace MetaData
{

bool readAsPosixTime(const std::string& str, boost::posix_time::ptime& time)
{
	const std::locale formats[] = {
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%m-%d")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%b-%d")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%B-%d")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y/%m/%d")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%d.%m.%Y")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y-%m")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y/%m")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y.%m")),
			std::locale(std::locale::classic(), new boost::posix_time::time_input_facet("%Y")),
	};
	for(size_t i=0; i < sizeof(formats)/sizeof(formats[0]); ++i)
	{
		std::istringstream iss(str);
		iss.imbue(formats[i]);
		if (iss >> time)
			return true;
	}

	return false;
}

bool readList(const std::string& str, const std::string& separators, std::list<std::string>& results)
{
	std::string curStr;

	BOOST_FOREACH(char c, str) {

		if (separators.find(c) != std::string::npos) {
			if (!curStr.empty()) {
				results.push_back(string_to_utf8(curStr));
				curStr.clear();
			}
		}
		else {
			if (curStr.empty() && std::isspace(c))
				continue;

			curStr.push_back(c);
		}
	}

	if (!curStr.empty())
		results.push_back(string_to_utf8(curStr));

	return !str.empty();
}


}

