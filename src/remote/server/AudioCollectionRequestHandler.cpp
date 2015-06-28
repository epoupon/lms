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
#include <boost/foreach.hpp>

#include "logger/Logger.hpp"

#include "AudioCollectionRequestHandler.hpp"

#include "database/Types.hpp"
#include "cover/CoverArtGrabber.hpp"

namespace Remote {
namespace Server {

using namespace Database;

AudioCollectionRequestHandler::AudioCollectionRequestHandler(Database::Handler& db)
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

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Database::Genre::pointer> genres = Database::Genre::getAll( _db.getSession(), request.batch_parameter().offset(), static_cast<int>(size));

	typedef Wt::Dbo::collection< Database::Genre::pointer > Genres;

	for (Genres::const_iterator it = genres.begin(); it != genres.end(); ++it)
	{
		AudioCollectionResponse_Genre* genre = response.add_genres();

		genre->set_name( std::string( boost::locale::conv::to_utf<char>((*it)->getName(), "UTF-8") ) );
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

	SearchFilter filter;

	// Get filters
	std::vector<std::string> genres;
	for (int id = 0; id < request.genre_size(); ++id)
		filter.exactMatch[SearchFilter::Field::Genre].push_back( request.genre(id) );

	// Now fetch requested data...

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<std::string> artists
		= Database::Track::getArtists(_db.getSession(), filter,
						request.batch_parameter().offset(), static_cast<int>(size) );

	BOOST_FOREACH(const std::string& artist, artists)
		response.add_artists()->set_name( std::string( boost::locale::conv::to_utf<char>(artist, "UTF-8") ) );

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

	SearchFilter filter;

	std::vector<std::string> artists;
	for (int id = 0; id < request.artist_size(); ++id)
		filter.exactMatch[SearchFilter::Field::Artist].push_back( request.artist(id) );

	std::vector<std::string> genres;
	for (int id = 0; id < request.genre_size(); ++id)
		filter.exactMatch[SearchFilter::Field::Genre].push_back( request.genre(id) );

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<std::string> releases
		= Database::Track::getReleases( _db.getSession(), filter,
						request.batch_parameter().offset(), static_cast<int>(size));

	BOOST_FOREACH(const std::string& release, releases)
	{
		response.add_releases()->set_name( std::string( boost::locale::conv::to_utf<char>(release, "UTF-8")));
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

	std::vector<std::string> artists;
	for (int id = 0; id < request.artist_size(); ++id)
		filter.exactMatch[SearchFilter::Field::Artist].push_back( request.artist(id) );

	std::vector<std::string> releases;
	for (int id = 0; id < request.release_size(); ++id)
		filter.exactMatch[SearchFilter::Field::Release].push_back( request.release(id) );

	std::vector<std::string> genres;
	for (int id = 0; id < request.genre_size(); ++id)
		filter.exactMatch[SearchFilter::Field::Genre].push_back( request.genre(id) );

	Wt::Dbo::Transaction transaction( _db.getSession() );

	std::vector<Database::Track::pointer> tracks
		= Database::Track::getAll( _db.getSession(), filter,
				request.batch_parameter().offset(), static_cast<int>(size));

	BOOST_FOREACH(Database::Track::pointer track, tracks)
	{
		AudioCollectionResponse_Track* newTrack = response.add_tracks();

		newTrack->set_id(track.id());
		newTrack->set_disc_number( track->getDiscNumber() );
		newTrack->set_track_number( track->getTrackNumber() );
		newTrack->set_artist( track->getArtistName() );
		newTrack->set_release( track->getReleaseName() );

		newTrack->set_name( std::string( boost::locale::conv::to_utf<char>(track->getName(), "UTF-8") ) );
		newTrack->set_duration_secs( track->getDuration().total_seconds() );

		// Only send the year part of the release times
		if (!track->getDate().is_special())
			newTrack->set_release_date( std::to_string(track->getDate().date().year()) );
		if (!track->getOriginalDate().is_special())
			newTrack->set_original_release_date( std::to_string(track->getOriginalDate().date().year()) );

		BOOST_FOREACH(Database::Genre::pointer genre, track->getGenres())
			newTrack->add_genre( genre->getName() );

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
			if (request.has_release())
			{
				coverArts = CoverArt::Grabber::instance().getFromRelease( _db.getSession(), request.release());
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

	BOOST_FOREACH(CoverArt::CoverArt& coverArt, coverArts)
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

	Database::MediaDirectorySettings::pointer settings = Database::MediaDirectorySettings::get( _db.getSession() );

	std::string hashStr = boost::posix_time::to_iso_string(settings->getLastUpdated());

	boost::uuids::detail::sha1 s;
	BOOST_FOREACH(const char c, hashStr)
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



} // namespace Remote
} // namespace Server


