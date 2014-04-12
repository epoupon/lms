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


		DatabaseHandler&	getDatabaseHandler()		{ return _db;}
		const DatabaseHandler&	getDatabaseHandler() const	{ return _db;}

	private:


		DatabaseHandler _db;
		std::string	_authenticatedUser;

};

} // namespace UserInterface

#endif

