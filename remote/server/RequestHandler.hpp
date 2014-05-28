#ifndef REMOTE_REQUEST_HANDLER
#define REMOTE_REQUEST_HANDLER

#include <boost/filesystem.hpp>

#include "messages/messages.pb.h"

#include "database/DatabaseHandler.hpp"

#include "AudioCollectionRequestHandler.hpp"
#include "MediaRequestHandler.hpp"

namespace Remote {
namespace Server {

class RequestHandler
{
	public:

		RequestHandler(boost::filesystem::path dbPath);

		bool process(const ClientMessage& request, ServerMessage& response);

		bool processAudioCollectionRequest(const AudioCollectionRequest& request, ServerMessage& response);

	private:

		DatabaseHandler _db;

		AudioCollectionRequestHandler	_audioCollectionRequestHandler;
		MediaRequestHandler		_mediaRequestHandler;
};

} // namespace Server
} // namespace Remote

#endif

