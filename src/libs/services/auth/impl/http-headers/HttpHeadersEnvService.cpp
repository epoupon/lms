/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "HttpHeadersEnvService.hpp"

#include <Wt/WEnvironment.h>

#include "utils/IConfig.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

namespace Auth
{

	HttpHeadersEnvService::HttpHeadersEnvService(Database::Db& db)
		: AuthServiceBase {db}
		, _fieldName {Service<IConfig>::get()->getString("http-headers-login-field", "X-Forwarded-User")}
	{
		LMS_LOG(AUTH, INFO) << "Using http header field = '" << _fieldName << "'";
	}

	HttpHeadersEnvService::CheckResult
	HttpHeadersEnvService::processEnv(const Wt::WEnvironment& env)
	{
		const std::string loginName {env.headerValue(_fieldName)};
		if (loginName.empty())
			return {CheckResult::State::Denied};

		LMS_LOG(AUTH, DEBUG) << "Extracted login name = '" << loginName <<  "' from HTTP header";

		const Database::UserId userId {getOrCreateUser(loginName)};
		onUserAuthenticated(userId);
		return {CheckResult::State::Granted, userId};
	}

	HttpHeadersEnvService::CheckResult
	HttpHeadersEnvService::processRequest(const Wt::Http::Request& request)
	{
		const std::string loginName {request.headerValue(_fieldName)};
		if (loginName.empty())
			return {CheckResult::State::Denied};

		LMS_LOG(AUTH, DEBUG) << "Extracted login name = '" << loginName <<  "' from HTTP header";

		const Database::UserId userId {getOrCreateUser(loginName)};
		onUserAuthenticated(userId);
		return {CheckResult::State::Granted, userId};
	}

} // namespace Auth

