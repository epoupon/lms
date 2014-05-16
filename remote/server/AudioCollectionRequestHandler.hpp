#ifndef REMOTE_AUDIO_COLLECTION_REQUEST_HANDLER
#define REMOTE_AUDIO_COLLECTION_REQUEST_HANDLER

#include "messages/messages.pb.h"

#include "database/DatabaseHandler.hpp"

namespace Remote {
namespace Server {

class AudioCollectionRequestHandler
{
	public:
		AudioCollectionRequestHandler(DatabaseHandler& db);

		bool process(const AudioCollectionRequest& request, AudioCollectionResponse& response);

	private:

		bool processGetArtists(const AudioCollectionRequest::GetArtistList& request, AudioCollectionResponse::ArtistList& response);

		DatabaseHandler& _db;

};

} // namespace Remote
} // namespace Server

#endif

