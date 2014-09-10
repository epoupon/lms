#ifndef METADATA_AVFORMAT_HPP
#define METADATA_AVFORMAT_HPP

#include "MetaData.hpp"

namespace MetaData
{

// Implements AVFORMAT library
class AvFormat : public Parser
{
	public:

		void parse(const boost::filesystem::path& p, Items& items);

	private:

};


} // namespace MetaData

#endif
