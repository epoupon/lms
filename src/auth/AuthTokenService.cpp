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

#include "AuthTokenService.hpp"

#include <Wt/Auth/HashFunction.h>
#include <Wt/Auth/PasswordStrengthValidator.h>
#include <Wt/WRandom.h>

#include "database/Session.hpp"
#include "utils/Exception.hpp"
#include "utils/Utils.hpp"
#include "utils/Logger.hpp"

namespace Auth {

AuthTokenService::AuthTokenService(std::size_t maxThrottlerEntries)
: _loginThrottler {maxThrottlerEntries}
{
}

std::string
AuthTokenService::createAuthToken(Database::Session& session, Database::IdType userId, const Wt::WDateTime& expiry)
{
	const std::string secret {Wt::WRandom::generateId(64)};

	auto transaction {session.createUniqueTransaction()};

	Database::User::pointer user {Database::User::getById(session, userId)};
	if (!user)
		throw LmsException {"User deleted"};

	Database::AuthToken::pointer authToken {Database::AuthToken::create(session, secret, expiry, user)};

	LMS_LOG(UI, DEBUG) << "Created auth token for user '" << user->getLoginName() << "', expiry = " << expiry.toString();

	if (user->getAuthTokensCount() >= 50)
		Database::AuthToken::removeExpiredTokens(session, Wt::WDateTime::currentDateTime());

	return secret;
}

static
boost::optional<AuthTokenService::AuthTokenProcessResult::AuthTokenInfo>
processAuthToken(Database::Session& session, const std::string& tokenValue)
{
	auto transaction {session.createUniqueTransaction()};

	Database::AuthToken::pointer authToken {Database::AuthToken::getByValue(session, tokenValue)};
	if (!authToken)
		return boost::none;

	if (authToken->getExpiry() < Wt::WDateTime::currentDateTime())
	{
		authToken.remove();
		return boost::none;
	}

	LMS_LOG(UI, DEBUG) << "Found auth token for user '" << authToken->getUser()->getLoginName() << "'!";

	AuthTokenService::AuthTokenProcessResult::AuthTokenInfo res {authToken->getUser().id(), authToken->getExpiry()};
	authToken.remove();

	return res;
}


AuthTokenService::AuthTokenProcessResult
AuthTokenService::processAuthToken(Database::Session& session, const boost::asio::ip::address& clientAddress, const std::string& tokenValue)
{
	// Do not waste too much resource on brute force attacks (optim)
	{
		std::shared_lock<std::shared_timed_mutex> lock {_mutex};

		if (_loginThrottler.isClientThrottled(clientAddress))
			return AuthTokenProcessResult {AuthTokenProcessResult::State::Throttled};
	}

	auto res {Auth::processAuthToken(session, tokenValue)};
	{
		std::unique_lock<std::shared_timed_mutex> lock {_mutex};

		if (_loginThrottler.isClientThrottled(clientAddress))
			return AuthTokenProcessResult {AuthTokenProcessResult::State::Throttled};

		if (!res)
		{
			_loginThrottler.onBadClientAttempt(clientAddress);
			return AuthTokenProcessResult {AuthTokenProcessResult::State::NotFound};
		}

		_loginThrottler.onGoodClientAttempt(clientAddress);
		return AuthTokenProcessResult {AuthTokenProcessResult::State::Found, std::move(*res)};
	}
}


} // namespace Auth

