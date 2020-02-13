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

#include "utils/Path.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <fstream>

#include <boost/crc.hpp>  // for boost::crc_32_type
#include <boost/tokenizer.hpp>

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

void
computeCrc(const std::filesystem::path& p, std::vector<unsigned char>& crc)
{
	using crc_type = boost::crc_32_type;
	crc_type result;

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
		LMS_LOG(DBUPDATER, ERROR) << "Failed to open file '" << p.string() << "'";
		throw LmsException("Failed to open file '" + p.string() + "'" );
	}


	// Copy the result into a vector of unsigned char
	const crc_type::value_type checksum = result.checksum();
	for (std::size_t i = 0; (i+1)*8 <= crc_type::bit_count; i++)
	{
		const unsigned char* data = reinterpret_cast<const unsigned char*>( &checksum );
		crc.push_back(data[i]);
	}
}

bool
ensureDirectory(const std::filesystem::path& dir)
{
	if (std::filesystem::exists(dir))
		return std::filesystem::is_directory(dir);
	else
		return std::filesystem::create_directory(dir);
}

Wt::WDateTime
getLastWriteTime(const std::filesystem::path& file)
{
	struct stat sb {};

	if (stat(file.string().c_str(), &sb) == -1)
		throw LmsException("Failed to get stats on file '" + file.string() + "'" );

	return Wt::WDateTime::fromTime_t(sb.st_mtime);
}

