
#include "RequestHandler.hpp"

namespace Remote {
namespace Server {

RequestHandler::RequestHandler(boost::filesystem::path dbPath)
: _db( dbPath ),
_audioCollectionRequestHandler(_db),
_mediaRequestHandler(_db)
{
}

bool
RequestHandler::process(const ClientMessage& request, ServerMessage& response)
{
	bool res = false;

	switch(request.type())
	{

		case ClientMessage_Type_AuthRequest:

			break;
		case ClientMessage_Type_AudioCollectionRequest:
			if (request.has_audio_collection_request())
			{
				res = _audioCollectionRequestHandler.process(request.audio_collection_request(), *response.mutable_audio_collection_response());
				if (res)
					response.set_type( ServerMessage::AudioCollectionResponse);
			}
			else
				std::cerr << "Malformed AudioCollectionRequest message!" << std::endl;
			break;

		case ClientMessage_Type_MediaRequest:
			if (request.has_media_request())
			{
				res = _mediaRequestHandler.process(request.media_request(), *response.mutable_media_response());
				if (res)
					response.set_type( ServerMessage::MediaResponse);
			}
			else
				std::cerr << "Malformed AudioCollectionRequest message!" << std::endl;
			break;
			break;

		default:
			std::cerr << "Unhandled message type = " << request.type() << std::endl;
	}

	return res;
}


} // namespace Server
} // namespace Remote



