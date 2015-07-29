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

#include <algorithm>    // std::min

#include <boost/locale.hpp>
#include <boost/uuid/sha1.hpp>

#include "logger/Logger.hpp"

#include "AudioCollectionRequestHandler.hpp"

#include "database/Types.hpp"
#include "cover/CoverArtGrabber.hpp"

namespace LmsAPI {
namespace Server {

using namespace Database;

static SearchFilter SearchFilterFromRequest(const AudioCollectionRequest_SearchFilter& request)
{
	SearchFilter filter;

	for (int id = 0; id < request.artist_id_size(); ++id)
		filter.idMatch[SearchFilter::Field::Artist].push_back( request.artist_id(id) );

	for (int id = 0; id < request.genre_id_size(); ++id)
		filter.idMatch[SearchFilter::Field::Genre].push_back( request.genre_id(id) );

	for (int id = 0; id < request.release_id_size(); ++id)
		filter.idMatch[SearchFilter::Field::Release].push_back( request.release_id(id) );

	for (int id = 0; id < request.track_id_size(); ++id)
		filter.idMatch[SearchFilter::Field::Track].push_back( request.track_id(id) );

	return filter;
}

AudioCollectionRequestHandler::AudioCollectionRequestHandler(Handler& db)
: _db(db)
{}

bool
AudioCollectionRequestHandler::process(const AudioCollectionRequest& request, AudioCollectionResponse& response)
{

	bool res = false;

	switch (request.type())
	{
		case AudioCollectionRequest::TypeGetRevision:
			// No payload
			res = processGetRevision(*response.mutable_revision());
			if (res)
				response.set_type(AudioCollectionResponse::TypeRevision);
			break;

		case AudioCollectionRequest::TypeGetGenreList:
			if (request.has_get_genres())
			{
				res = processGetGenres(request.get_genres(), *response.mutable_genre_list());
				if (res)
					response.set_type(AudioCollectionResponse::TypeArtistList);
			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad AudioCollectionRequest::TypeGetGenreList";
			break;

		case AudioCollectionRequest::TypeGetArtistList:
			if (request.has_get_artists())
			{
				res = processGetArtists(request.get_artists(), *response.mutable_artist_list());
				if (res)
					response.set_type(AudioCollectionResponse::TypeArtistList);

			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad AudioCollectionRequest::TypeGetArtistList message!";
			break;

		case AudioCollectionRequest::TypeGetReleaseList:
			if (request.has_get_releases())
			{
				res = processGetReleases(request.get_releases(), *response.mutable_release_list());
				if (res)
					response.set_type(AudioCollectionResponse::TypeReleaseList);

			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad AudioCollectionRequest::TypeGetReleaseList message!";
			break;

		case AudioCollectionRequest::TypeGetTrackList:
			if (request.has_get_tracks())
			{
				res = processGetTracks(request.get_tracks(), *response.mutable_track_list());
				if (res)
					response.set_type(AudioCollectionResponse::TypeTrackList);

			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad AudioCollectionRequest::TypeGetTrackList message!";
			break;

		case AudioCollectionRequest::TypeGetCoverArt:
			if (request.has_get_cover_art())
				res = processGetCoverArt(request.get_cover_art(), response);
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad AudioCollectionRequest::TypeGetCoverArt message!";
			break;

		default:
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Unhandled AudioCollectionRequest_Type = " << request.type();

	}

	return res;
}


bool
AudioCollectionRequestHandler::processGetGenres(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "No batch parameters found!";
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListArtists;
	size = std::min(size, _maxListArtists);

	// Get filters
	SearchFilter filter;
	if (request.has_search_filter())
		filter = SearchFilterFromRequest(request.search_filter());

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<Genre::pointer> genres = Genre::getByFilter( _db.getSession(), filter, request.batch_parameter().offset(), static_cast<int>(size));

	for (Genre::pointer genre : genres)
	{
		AudioCollectionResponse_Genre* addGenre = response.add_genres();

		addGenre->set_id(genre.id());
		addGenre->set_name( std::string( boost::locale::conv::to_utf<char>(genre->getName(), "UTF-8") ) );
	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetArtists(const AudioCollectionRequest::GetArtistList& request, AudioCollectionResponse::ArtistList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "No batch parameters found!";
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListArtists;
	size = std::min(size, _maxListArtists);

	// Get filters
	SearchFilter filter;
	if (request.has_search_filter())
		filter = SearchFilterFromRequest(request.search_filter());

	// Now fetch requested data...

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<Artist::pointer> artists
		= Artist::getByFilter(_db.getSession(), filter,
						request.batch_parameter().offset(), static_cast<int>(size) );

	for (Artist::pointer artist : artists)
	{
		AudioCollectionResponse_Artist *addArtist = response.add_artists();

		addArtist->set_id(artist.id());
		addArtist->set_name( std::string( boost::locale::conv::to_utf<char>(artist->getName(), "UTF-8") ) );
		if (!artist->getMBID().empty())
			addArtist->set_mbid(artist->getMBID());
	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetReleases(const AudioCollectionRequest::GetReleaseList& request, AudioCollectionResponse::ReleaseList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "No batch parameters found!";
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListReleases;
	size = std::min(size, _maxListReleases);

	// Get filters
	SearchFilter filter;
	if (request.has_search_filter())
		filter = SearchFilterFromRequest(request.search_filter());

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<Release::pointer> releases
		= Release::getByFilter( _db.getSession(), filter,
						request.batch_parameter().offset(), static_cast<int>(size));

	for (Release::pointer release : releases)
	{
		AudioCollectionResponse_Release *addRelease = response.add_releases();

		addRelease->set_id(release.id());
		addRelease->set_name( std::string( boost::locale::conv::to_utf<char>(release->getName(), "UTF-8")));
		if (!release->getMBID().empty())
			addRelease->set_mbid(release->getMBID());
	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetTracks(const AudioCollectionRequest::GetTrackList& request, AudioCollectionResponse::TrackList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "No batch parameters found!";
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListTracks;
	size = std::min(size, _maxListTracks);

	// Get filters
	SearchFilter filter;
	if (request.has_search_filter())
		filter = SearchFilterFromRequest(request.search_filter());

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<Track::pointer> tracks
		= Track::getByFilter( _db.getSession(), filter,
				request.batch_parameter().offset(), static_cast<int>(size));

	for (Track::pointer track : tracks)
	{
		AudioCollectionResponse_Track* newTrack = response.add_tracks();

		newTrack->set_id(track.id());
		newTrack->set_disc_number( track->getDiscNumber() );
		newTrack->set_track_number( track->getTrackNumber() );
		newTrack->set_artist_id( track->getArtist().id() );
		newTrack->set_release_id( track->getRelease().id() );

		newTrack->set_name( std::string( boost::locale::conv::to_utf<char>(track->getName(), "UTF-8") ) );
		newTrack->set_duration_secs( track->getDuration().total_seconds() );

		// Only send the year part of the release times
		if (!track->getDate().is_special())
			newTrack->set_release_date( std::to_string(track->getDate().date().year()) );
		if (!track->getOriginalDate().is_special())
			newTrack->set_original_release_date( std::to_string(track->getOriginalDate().date().year()) );
		if (!track->getMBID().empty())
			newTrack->set_mbid(track->getMBID());

		for (Genre::pointer genre : track->getGenres())
			newTrack->add_genre_id( genre.id() );

	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetCoverArt(const AudioCollectionRequest::GetCoverArt& request, AudioCollectionResponse& response)
{
	bool res = false;

	response.set_type(AudioCollectionResponse::TypeCoverArt);

	std::vector<CoverArt::CoverArt> coverArts;

	switch(request.type())
	{
		case AudioCollectionRequest::GetCoverArt::TypeGetCoverArtRelease:
			if (request.has_release_id())
			{
				coverArts = CoverArt::Grabber::instance().getFromRelease( _db.getSession(), request.release_id());
				res = true;
			}
			break;

		case AudioCollectionRequest::GetCoverArt::TypeGetCoverArtTrack:
			if (request.has_track_id())
			{
				coverArts = CoverArt::Grabber::instance().getFromTrack(_db.getSession(), request.track_id());
				res = true;
			}
			break;
	}

	for (CoverArt::CoverArt& coverArt : coverArts)
	{
		AudioCollectionResponse_CoverArt* cover_art = response.add_cover_art();

		if (request.has_size())
		{
			std::size_t size = request.size();
			if (size > _maxCoverArtSize || size == 0)
				size = _maxCoverArtSize;
			if (size < _minCoverArtSize)
				size = _minCoverArtSize;

			coverArt.scale(size);
		}

		cover_art->set_mime_type(coverArt.getMimeType());
		cover_art->set_data( std::string( coverArt.getData().begin(), coverArt.getData().end()) );
	}

	return res;
}

bool
AudioCollectionRequestHandler::processGetRevision(AudioCollectionResponse::Revision& response)
{

	bool res = false;

	Wt::Dbo::Transaction transaction( _db.getSession() );

	MediaDirectorySettings::pointer settings = MediaDirectorySettings::get( _db.getSession() );

	std::string hashStr = boost::posix_time::to_iso_string(settings->getLastUpdated());

	boost::uuids::detail::sha1 s;
	for (const char c : hashStr)
		s.process_byte(c);

	unsigned int digest[5];
	s.get_digest(digest);

	std::ostringstream oss;
	for (std::size_t i = 0; i < 5; ++i)
		oss << std::hex << std::setfill('0') << std::setw(4) << digest[i];

	response.set_rev(oss.str());

	res = true;

	return res;
}



} // namespace LmsAPI
} // namespace Server


