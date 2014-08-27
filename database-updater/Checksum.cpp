#include <fstream>
#include <stdexcept>

#include <boost/crc.hpp>  // for boost::crc_32_type

#include "logger/Logger.hpp"

#include "Checksum.hpp"

typedef boost::crc_32_type crc_type;

void computeCrc(const boost::filesystem::path& p, std::vector<unsigned char>& crc)
{
	crc_type	result;

	std::ifstream  ifs( p.string().c_str(), std::ios_base::binary );

	if (ifs)
	{
		do
		{
			std::array<char,1024>	buffer;

			ifs.read( buffer.data(), buffer.size() );
			result.process_bytes( buffer.data(), ifs.gcount() );
		}
		while ( ifs );
	}
	else
	{
		LMS_LOG(MOD_DBUPDATER, SEV_ERROR) << "Failed to open file '" << p << "'";
		throw std::runtime_error("Failed to open file '" + p.string() + "'" );
	}

	// Copy back result into the vector

	// Copy the result into a vector of unsigned char
	const crc_type::value_type checksum = result.checksum();
	for (std::size_t i = 0; (i+1)*8 <= crc_type::bit_count; i++)
	{
		const unsigned char* data = reinterpret_cast<const unsigned char*>( &checksum );
		crc.push_back(data[i]);
	}

}

