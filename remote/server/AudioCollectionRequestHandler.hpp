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
		bool processGetReleases(const AudioCollectionRequest::GetReleaseList& request, AudioCollectionResponse::ReleaseList& response);
		bool processGetTracks(const AudioCollectionRequest::GetTrackList& request, AudioCollectionResponse::TrackList& response);

		DatabaseHandler& _db;

		static const std::size_t _maxListArtists = 256;
		static const std::size_t _maxListGenres = 256;
		static const std::size_t _maxListReleases = 256;
		static const std::size_t _maxListTracks = 1024;
};

} // namespace Remote
} // namespace Server

#endif

