/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef REMOTE_AUDIO_COLLECTION_REQUEST_HANDLER
#define REMOTE_AUDIO_COLLECTION_REQUEST_HANDLER

#include "database/DatabaseHandler.hpp"

#include "messages.pb.h"

namespace Remote {
namespace Server {

class AudioCollectionRequestHandler
{
	public:
		AudioCollectionRequestHandler(Database::Handler& db);

		bool process(const AudioCollectionRequest& request, AudioCollectionResponse& response);

	private:

		bool processGetRevision(const AudioCollectionRequest::GetRevision& request, AudioCollectionResponse::Revision& response);
		bool processGetArtists(const AudioCollectionRequest::GetArtistList& request, AudioCollectionResponse::ArtistList& response);
		bool processGetGenres(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response);
		bool processGetReleases(const AudioCollectionRequest::GetReleaseList& request, AudioCollectionResponse::ReleaseList& response);
		bool processGetTracks(const AudioCollectionRequest::GetTrackList& request, AudioCollectionResponse::TrackList& response);
		bool processGetCoverArt(const AudioCollectionRequest::GetCoverArt& request, AudioCollectionResponse& response);

		Database::Handler& _db;

		static const std::size_t _maxListArtists = 256;
		static const std::size_t _maxListGenres = 256;
		static const std::size_t _maxListReleases = 128;
		static const std::size_t _maxListTracks = 128;

		static const std::size_t _minCoverArtSize = 64;		// in pixels, square
		static const std::size_t _maxCoverArtSize = 512;	// in pixels, square
};

} // namespace Remote
} // namespace Server

#endif

