#include "database/DatabaseHandler.hpp"
#include <Wt/Auth/PasswordService>
#include <Wt/Auth/Identity>

int main(void)
{
	try
	{
		boost::filesystem::remove("test_user.db");

		// Set up the database session
		Database::Handler::configureAuth();

		Database::Handler db("test_user.db");

		Wt::Dbo::Transaction transaction(db.getSession());

		Wt::Auth::Identity identity;
		Wt::Auth::User user = db.getUserDatabase().registerNew();
		std::cout << "User is valid = " << std::boolalpha << user.isValid() << std::endl;

		user.setIdentity(identity.provider(), "toto");

		std::cout << "User is valid = " << std::boolalpha << user.isValid() << std::endl;

		std::cout << "Updating password" << std::endl;
		db.getPasswordService().updatePassword(user, "This is my password");

//		db.getSession().add( user );

		std::cout << "Committing" << std::endl;

		transaction.commit();

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

