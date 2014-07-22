#ifndef UI_AUTH_HPP
#define UI_AUTH_HPP

#include <Wt/Auth/AuthService>
#include <Wt/Auth/AbstractUserDatabase>
#include <Wt/Auth/Login>
#include <Wt/Auth/AuthWidget>
#include <Wt/WContainerWidget>

#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class LmsAuth : public Wt::Auth::AuthWidget
{
	public:

		LmsAuth(Database::Handler& db);

		// LoggedInView is delegated to LmsHome
		void createLoggedInView () ;
		void createLoginView ();

	private:


};

} // namespace UserInterface

#endif

