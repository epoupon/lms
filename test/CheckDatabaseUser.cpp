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

