#ifndef UI_SESSION_DATA_HPP
#define UI_SESSION_DATA_HPP

#include <boost/filesystem.hpp>

#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class SessionData
{
	public:

		SessionData(boost::filesystem::path dbPath);

		Database::Handler&		getDatabaseHandler()		{ return _db;}
		const Database::Handler&	getDatabaseHandler() const	{ return _db;}

	private:

		Database::Handler	_db;

};

} // namespace UserInterface

#endif

