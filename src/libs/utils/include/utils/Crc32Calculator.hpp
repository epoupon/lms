/*
 * Copyright (C) 2020 Emeric Poupon
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

#pragma once

#include <boost/crc.hpp>  // for boost::crc_32_type

namespace Utils
{

	class Crc32Calculator
	{
		public:

			void processBytes(const std::byte* _data, std::size_t dataSize)
			{
				_result.process_bytes(_data, dataSize);
			}

			std::uint32_t getResult() const
			{
				return _result.checksum();
			}

		private:
			using Crc32Type = boost::crc_32_type;
			Crc32Type _result;
	};

}

