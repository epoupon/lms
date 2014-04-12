#ifndef REMOTE_REQUEST_HANDLER
#define REMOTE_REQUEST_HANDLER

#include <boost/filesystem.hpp>

#include "database/DatabaseHandler.hpp"

// #include "remote/messages/

namespace Remote {
namespace Server {

class RequestHandler
{
	public:

		RequestHandler(boost::filesystem::path dbPath);


	private:

		DatabaseHandler _db;

};

} // namespace Server
} // namespace Remote

#endif

