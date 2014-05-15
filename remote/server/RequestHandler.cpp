
#include "RequestHandler.hpp"

namespace Remote {
namespace Server {

RequestHandler::RequestHandler(boost::filesystem::path dbPath)
: _db( dbPath ),
_audioCollectionRequestHandler(_db)
{
}

bool
RequestHandler::process(const ClientMessage& request, std::vector<ServerMessage> responses)
{
	std::cout << "TODO: process request!" << std::endl;

	switch(request.type())
	{

		case ClientMessage_Type_AuthRequest:

			break;
		case ClientMessage_Type_AudioCollectionRequest:
			if (request.has_audio_collection_request())
				return _audioCollectionRequestHandler.process(request.audio_collection_request(), responses);
			else
				std::cerr << "Malformed AudioCollectionRequest message!" << std::endl;
			break;

		case ClientMessage_Type_MediaRequest:
			break;

		default:
			std::cerr << "Unhandled message type = " << request.type() << std::endl;
	}

	return false;
}


} // namespace Server
} // namespace Remote



