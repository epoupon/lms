#ifndef REMOTE_REQUEST_HANDLER
#define REMOTE_REQUEST_HANDLER

#include <boost/filesystem.hpp>

#include "messages/messages.pb.h"

#include "database/DatabaseHandler.hpp"

#include "AudioCollectionRequestHandler.hpp"

namespace Remote {
namespace Server {

class RequestHandler
{
	public:

		RequestHandler(boost::filesystem::path dbPath);

		bool process(const ClientMessage& request, std::vector<ServerMessage> responses);

		bool processAudioCollectionRequest(const AudioCollectionRequest& request, std::vector<ServerMessage> responses);

	private:

		DatabaseHandler _db;

		AudioCollectionRequestHandler _audioCollectionRequestHandler;

};

} // namespace Server
} // namespace Remote

#endif

