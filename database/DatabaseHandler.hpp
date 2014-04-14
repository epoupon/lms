#ifndef DATABASE_HANDLER_HPP
#define DATABASE_HANDLER_HPP

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/backend/Sqlite3>

// Long living class handling the database
class DatabaseHandler
{
	public:
		DatabaseHandler(boost::filesystem::path db);

		Wt::Dbo::Session& getSession() { return _session; }

		boost::filesystem::path getPath() const { return _path; }


	private:

		boost::filesystem::path		_path;
		Wt::Dbo::backend::Sqlite3	_dbBackend;
		Wt::Dbo::Session		_session;

};


#endif

