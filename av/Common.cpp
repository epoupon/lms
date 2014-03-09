#include "Common.hpp"

#include <boost/array.hpp>

std::string
AvError::to_str(void) const
{
	boost::array<char, 128> buf = {0};
	if (av_strerror(_errnum, buf.data(), buf.size()) == 0)
		return std::string(&buf[0]);
	else
		return "Unknown error";

}


std::ostream& operator<<(std::ostream& ost, const AvError& err)
{
	ost << err.to_str();
	return ost;
}

void AvInit()
{
	/* register all the codecs */
	avcodec_register_all();
	av_register_all();
	std::cout << "AVCDOEC VERSION = " << avcodec_version() << std::endl;
}
