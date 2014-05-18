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
		bool processGetGenres(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response);

		DatabaseHandler& _db;

		static const std::size_t _maxListArtists = 128;
		static const std::size_t _maxListGenres = 128;
		static const std::size_t _maxListReleases = 128;
		static const std::size_t _maxListTracks = 32;
};

} // namespace Remote
} // namespace Server

#endif

