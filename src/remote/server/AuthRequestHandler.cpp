/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <Wt/Auth/Identity>

#include "logger/Logger.hpp"

#include "AuthRequestHandler.hpp"

namespace Remote {
namespace Server {

AuthRequestHandler::AuthRequestHandler(Database::Handler& db)
: _db(db)
{
}

bool
AuthRequestHandler::process(const AuthRequest& request, AuthResponse& response)
{
	bool res = false;

	switch (request.type())
	{
		case AuthRequest::TypePassword:
			if (request.has_password())
			{
				res = processPassword(request.password(), *response.mutable_password_result());
				if (res)
					response.set_type(AuthResponse::TypePasswordResult);
			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad AuthRequest::TypePassword";
			break;
	}

	return res;
}


bool
AuthRequestHandler::processPassword(const AuthRequest::Password& request, AuthResponse::PasswordResult& response)
{
	bool res = false;

	// Get the user
	const Wt::Auth::User& user = _db.getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, request.user_login());
	if (user.isValid())
	{
		// Now attempt to log the user in

		Wt::Auth::PasswordResult result = _db.getPasswordService().verifyPassword(user, request.user_password());
		switch( result )
		{
			case Wt::Auth::PasswordInvalid:
				response.set_type(AuthResponse::PasswordResult::TypePasswordInvalid);
				LMS_LOG(MOD_REMOTE, SEV_NOTICE) << "User '" << request.user_login() << "': invalid password";
				res = true;
				break;
			case Wt::Auth::LoginThrottling:
				response.set_type(AuthResponse::PasswordResult::TypeLoginThrottling);
				response.set_delay(_db.getPasswordService().delayForNextAttempt(user));
				res = true;
				break;
			case Wt::Auth::PasswordValid:
				response.set_type(AuthResponse::PasswordResult::TypePasswordValid);
				// Log the user in
				_db.getLogin().login( user );
				LMS_LOG(MOD_REMOTE, SEV_NOTICE) << "User '" << request.user_login() << "' successfully logged in";
				res = true;
				break;
			default:
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Cannot handle password result!";
				break;
		}

	}
	else
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Invalid user '" << request.user_login();
		response.set_type(AuthResponse::PasswordResult::TypePasswordInvalid);
	}

	return res;
}

} // namespace Remote
} // namespace Server

