#ifndef REMOTE_AUTH_REQUEST_HANDLER
#define REMOTE_AUTH_REQUEST_HANDLER

#include "messages/messages.pb.h"

#include "database/DatabaseHandler.hpp"

namespace Remote {
namespace Server {

class AuthRequestHandler
{
	public:
		AuthRequestHandler(Database::Handler& db);

		bool process(const AuthRequest& request, AuthResponse& response);

	private:

		bool processPassword(const AuthRequest::Password& request, AuthResponse::PasswordResult& response);

		Database::Handler& _db;
};

} // namespace Remote
} // namespace Server

#endif
