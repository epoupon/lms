/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "utils/NetAddress.hpp"

#ifndef BOOST_ASIO_HAS_STD_HASH

namespace std
{
	std::size_t hash<boost::asio::ip::address>::operator()(const boost::asio::ip::address& ipAddr) const
	{
		if (ipAddr.is_v4())
			return ipAddr.to_v4().to_ulong();

		if (ipAddr.is_v6())
		{
			const auto& range {ipAddr.to_v6().to_bytes()};
			std::size_t res {};

			for (auto b : range)
				res ^= std::hash<char>{}(static_cast<char>(b));

			return res;
		}

		return std::hash<std::string>{}(ipAddr.to_string());
	}
}

#endif // BOOST_ASIO_HAS_STD_HASH
