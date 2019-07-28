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

/* This file contains some classes in order to get info from file using the libavconv */

#pragma once

#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <Wt/WDateTime.h>

#include "utils/NetAddress.hpp"
#include "utils/Exception.hpp"

namespace Auth {

class LoginThrottler
{
	public:
		LoginThrottler(std::size_t maxEntries) : _maxEntries {maxEntries} {}

		// user must lock these calls to avoid races
		bool isClientThrottled(const boost::asio::ip::address& address) const;
		void onBadClientAttempt(const boost::asio::ip::address& address);
		void onGoodClientAttempt(const boost::asio::ip::address& address);

	private:

		void removeOutdatedEntries();

		const std::size_t _maxEntries;

		std::unordered_map<boost::asio::ip::address, Wt::WDateTime>	_attemptsInfo;
};


} // Auth

