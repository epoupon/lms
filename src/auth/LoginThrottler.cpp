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

#include "LoginThrottler.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace Auth {

static
boost::asio::ip::address_v6
getAddressWithMask(const boost::asio::ip::address_v6& address, std::size_t prefix)
{
	assert(prefix % 8 == 0);

	std::array<uint8_t, 16> truncatedBytes;

	auto bytes {address.to_bytes()};
	std::copy(std::cbegin(bytes), std::next(std::cbegin(bytes), prefix / 8), truncatedBytes.begin());

	return boost::asio::ip::address_v6 {truncatedBytes};
}

static
boost::asio::ip::address
getAddressToThrottle(const boost::asio::ip::address& address)
{
	return address.is_v6() ? getAddressWithMask(address.to_v6(), 64) : address;
}

void
LoginThrottler::removeOutdatedEntries()
{
	const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};

	for (auto it {std::begin(_attemptsInfo)}; it != std::end(_attemptsInfo); )
	{
		if (it->second.nextValidAttempt <= now)
			it = _attemptsInfo.erase(it);
		else
			++it;
	}
}

void
LoginThrottler::onBadClientAttempt(const boost::asio::ip::address& address)
{
	const boost::asio::ip::address clientAddress {getAddressToThrottle(address)};

	const Wt::WDateTime now {Wt::WDateTime::currentDateTime()};

	std::unique_lock<std::shared_timed_mutex> lock {_mutex};

	if (_attemptsInfo.size() >= _maxEntries)
		removeOutdatedEntries();
	// If still full, kill one random entry
	if (_attemptsInfo.size() >= _maxEntries)
		_attemptsInfo.erase(pickRandom(_attemptsInfo));

	AttemptsInfo& attemptsInfo {_attemptsInfo[address]};

	attemptsInfo.nbSuccessiveBadAttempts++;

	if (attemptsInfo.nbSuccessiveBadAttempts >= 50)
		attemptsInfo.nextValidAttempt = now.addSecs(60);
	if (attemptsInfo.nbSuccessiveBadAttempts >= 20)
		attemptsInfo.nextValidAttempt = now.addSecs(10);
	else if (attemptsInfo.nbSuccessiveBadAttempts >= 10)
		attemptsInfo.nextValidAttempt = now.addSecs(5);
	else if (attemptsInfo.nbSuccessiveBadAttempts >= 5)
		attemptsInfo.nextValidAttempt = now.addSecs(2);
	else if (attemptsInfo.nbSuccessiveBadAttempts >= 2)
		attemptsInfo.nextValidAttempt = now.addSecs(1);
	else
		attemptsInfo.nextValidAttempt = now;

	LMS_LOG(AUTH, INFO) << "Registering bad attempt for '" << clientAddress.to_string() << "' (" << attemptsInfo.nbSuccessiveBadAttempts << " successive bad attempts)";
}

void
LoginThrottler::onGoodClientAttempt(const boost::asio::ip::address& address)
{
	const boost::asio::ip::address clientAddress {getAddressToThrottle(address)};

	std::unique_lock<std::shared_timed_mutex> lock {_mutex};

	_attemptsInfo.erase(address);
}

bool
LoginThrottler::isClientThrottled(const boost::asio::ip::address& address) const
{
	const boost::asio::ip::address clientAddress {getAddressToThrottle(address)};

	std::shared_lock<std::shared_timed_mutex> lock {_mutex};

	auto it {_attemptsInfo.find(address)};
	if (it == _attemptsInfo.end())
		return false;

	return it->second.nextValidAttempt > Wt::WDateTime::currentDateTime();
}

} // Auth

