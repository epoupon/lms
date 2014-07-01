#ifndef UI_SESSION_DATA_HPP
#define UI_SESSION_DATA_HPP

#include <string>

#include <boost/filesystem.hpp>

#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class SessionData
{
	public:

		SessionData(boost::filesystem::path dbPath);

		void setAuthenticatedUser(std::string user);


		Database::Handler&		getDatabaseHandler()		{ return _db;}
		const Database::Handler&	getDatabaseHandler() const	{ return _db;}

	private:


		Database::Handler	_db;
		std::string		_authenticatedUser;

};

} // namespace UserInterface

#endif

