#include <Wt/Auth/Identity>

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
				std::cerr << "Bad AuthRequest::TypePassword" << std::endl;
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
				res = true;
				break;
			default:
				break;
		}

	}
	else
	{
		std::cerr << "Invalid user '" << request.user_login() << std::endl;
		response.set_type(AuthResponse::PasswordResult::TypePasswordInvalid);
	}

	return res;
}

} // namespace Remote
} // namespace Server

