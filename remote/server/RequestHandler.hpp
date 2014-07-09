#ifndef REMOTE_REQUEST_HANDLER
#define REMOTE_REQUEST_HANDLER

#include <boost/filesystem.hpp>

#include "messages/messages.pb.h"

#include "database/DatabaseHandler.hpp"

#include "AuthRequestHandler.hpp"
#include "AudioCollectionRequestHandler.hpp"
#include "MediaRequestHandler.hpp"

namespace Remote {
namespace Server {

class RequestHandler
{
	public:

		RequestHandler(boost::filesystem::path dbPath);
		~RequestHandler();

		bool process(const ClientMessage& request, ServerMessage& response);

	private:

		Database::Handler _db;

		AuthRequestHandler		_authRequestHandler;
		AudioCollectionRequestHandler	_audioCollectionRequestHandler;
		MediaRequestHandler		_mediaRequestHandler;
};

} // namespace Server
} // namespace Remote

#endif

