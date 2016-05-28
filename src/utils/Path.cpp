/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <fstream>
#include <stdexcept>

#include <boost/crc.hpp>  // for boost::crc_32_type
#include <boost/tokenizer.hpp>

#include "logger/Logger.hpp"

#include "Path.hpp"

boost::filesystem::path searchExecPath(std::string filename)
{
	std::string path;

	path = ::getenv("PATH");
	if (path.empty())
		throw std::runtime_error("Environment variable PATH not found");

	std::string result;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(":");
	tokenizer tok(path, sep);
	for (tokenizer::iterator it = tok.begin(); it != tok.end(); ++it)
	{
		boost::filesystem::path p = *it;
		p /= filename;
		if (!::access(p.c_str(), X_OK))
		{
			result = p.string();
			break;
		}
	}
	return result;
}

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
		LMS_LOG(DBUPDATER, ERROR) << "Failed to open file '" << p << "'";
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

bool ensureDirectory(boost::filesystem::path dir)
{
	if (boost::filesystem::exists(dir))
		return boost::filesystem::is_directory(dir);
	else
		return boost::filesystem::create_directory(dir);
}

