#ifndef DATABASE_HANDLER_HPP
#define DATABASE_HANDLER_HPP

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/backend/Sqlite3>
#include <Wt/Auth/Dbo/UserDatabase>
#include <Wt/Auth/Login>
#include <Wt/Auth/PasswordService>

#include "User.hpp"

namespace Database {

		typedef Wt::Auth::Dbo::UserDatabase<AuthInfo> UserDatabase;

// Session living class handling the database
class Handler
{
	public:

		Handler(boost::filesystem::path db);
		~Handler();

		Wt::Dbo::Session& getSession() { return _session; }

		User::pointer getCurrentUser();	// get the current user, may return empty
		User::pointer getUser(const Wt::Auth::User& authUser);	// Get or create the given user

		Wt::Auth::AbstractUserDatabase& getUserDatabase();
		Wt::Auth::Login& getLogin() { return _login; }

		// Long living shared associated services
		static void configureAuth();

		static const Wt::Auth::AuthService& getAuthService();
		static const Wt::Auth::PasswordService& getPasswordService();


	private:

		Wt::Dbo::backend::Sqlite3	_dbBackend;
		Wt::Dbo::Session		_session;

		UserDatabase*			_users;
		Wt::Auth::Login 		_login;

};

} // namespace Database

#endif

