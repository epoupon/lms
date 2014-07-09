
#include "RequestHandler.hpp"

namespace Remote {
namespace Server {

RequestHandler::RequestHandler(boost::filesystem::path dbPath)
: _db( dbPath ),
_authRequestHandler(_db),
_audioCollectionRequestHandler(_db),
_mediaRequestHandler(_db)
{
}

RequestHandler::~RequestHandler()
{
	// TODO manually log out user if needed?
	_db.getLogin().logout();
}

bool
RequestHandler::process(const ClientMessage& request, ServerMessage& response)
{
	bool res = false;

	switch(request.type())
	{

		case ClientMessage::AuthRequest:
			if (request.has_auth_request())
			{
				res = _authRequestHandler.process(request.auth_request(), *response.mutable_auth_response());
				if (res)
					response.set_type(ServerMessage::AuthResponse);
			}
			else
				std::cerr << "Bad ClientMessage::AuthRequest !" << std::endl;
			break;
		case ClientMessage::AudioCollectionRequest:
			// Not allowed if the user is not logged in
			if (_db.getLogin().loggedIn())
			{
				if (request.has_audio_collection_request())
				{
					res = _audioCollectionRequestHandler.process(request.audio_collection_request(), *response.mutable_audio_collection_response());
					if (res)
						response.set_type( ServerMessage::AudioCollectionResponse);
				}
				else
					std::cerr << "Bad ClientMessage::AudioCollectionRequest message!" << std::endl;
			}
			break;

		case ClientMessage::MediaRequest:
			// Not allowed if the user is not logged in
			if (_db.getLogin().loggedIn())
			{
				if (request.has_media_request())
				{
					res = _mediaRequestHandler.process(request.media_request(), *response.mutable_media_response());
					if (res)
						response.set_type( ServerMessage::MediaResponse);
				}
				else
					std::cerr << "Malformed ClientMessage::MediaRequest message!" << std::endl;
			}
			break;

		default:
			std::cerr << "Unhandled message type = " << request.type() << std::endl;
	}

	return res;
}


} // namespace Server
} // namespace Remote



