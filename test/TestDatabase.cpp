
#include "TestDatabase.hpp"

namespace TestDatabase {

	Database::Handler* create()
{

	boost::filesystem::path p ("test_db");

	// Remove previous db
//	boost::filesystem::remove(p);

	Database::Handler* db = new Database::Handler(p);


	// Populate DB

	// Add artist

	// Add


	return db;
}

} // namespace TestDatabase
