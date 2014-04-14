
#include "TestDatabase.hpp"

namespace TestDatabase {

DatabaseHandler* create()
{

	boost::filesystem::path p ("test_db");

	// Remove previous db
	boost::filesystem::remove(p);

	DatabaseHandler* db = new DatabaseHandler(p);


	// Populate DB

	// Add artist

	// Add


	return db;
}

} // namespace TestDatabase
