#include <algorithm>    // std::min

#include <boost/foreach.hpp>

#include "AudioCollectionRequestHandler.hpp"

#include "database/AudioTypes.hpp"

namespace Remote {
namespace Server {

AudioCollectionRequestHandler::AudioCollectionRequestHandler(DatabaseHandler& db)
: _db(db)
{}


bool
AudioCollectionRequestHandler::process(const AudioCollectionRequest& request, AudioCollectionResponse& response)
{

	bool res = false;

	switch (request.type())
	{
		case AudioCollectionRequest_Type_TypeGetGenreList:
			if (request.has_get_genres())
			{
				res = processGetGenres(request.get_genres(), *response.mutable_genre_list());
				if (res)
					response.set_type(AudioCollectionResponse_Type_TypeArtistList);
			}
			else
				std::cerr << "Bad AudioCollectionRequest_Type_TypeGetGenreList" << std::endl;
			break;

		case AudioCollectionRequest_Type_TypeGetArtistList:
			if (request.has_get_artists())
			{
				res = processGetArtists(request.get_artists(), *response.mutable_artist_list());
				if (res)
					response.set_type(AudioCollectionResponse_Type_TypeArtistList);

			}
			else
				std::cerr << "Bad AudioCollectionRequest_Type_TypeGetArtistList" << std::endl;
			break;

		case AudioCollectionRequest_Type_TypeGetReleaseList:
			if (request.has_get_releases())
			{
				res = processGetReleases(request.get_releases(), *response.mutable_release_list());
				if (res)
					response.set_type(AudioCollectionResponse_Type_TypeReleaseList);

			}
			else
				std::cerr << "Bad AudioCollectionRequest_Type_TypeGetReleaseList" << std::endl;
			break;

		case AudioCollectionRequest_Type_TypeGetTrackList:
			if (request.has_get_tracks())
			{
				res = processGetTracks(request.get_tracks(), *response.mutable_track_list());
				if (res)
					response.set_type(AudioCollectionResponse_Type_TypeTrackList);

			}
			else
				std::cerr << "Bad AudioCollectionRequest_Type_TypeGetTrackList" << std::endl;
			break;

		default:
			std::cerr << "Unhandled AudioCollectionRequest_Type = " << request.type() << std::endl;
	}

	return res;
}


bool
AudioCollectionRequestHandler::processGetGenres(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		std::cerr << "No batch parameters found!" << std::endl;
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListArtists;
	size = std::min(size, _maxListArtists);

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Genre::pointer> genres = Genre::getAll( _db.getSession(), request.batch_parameter().offset(), static_cast<int>(size));

	typedef Wt::Dbo::collection< Genre::pointer > Genres;

	for (Genres::const_iterator it = genres.begin(); it != genres.end(); ++it)
	{
		AudioCollectionResponse_Genre* genre = response.add_genres();

		genre->set_name((*it)->getName());
		genre->set_id(it->id());
	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetArtists(const AudioCollectionRequest::GetArtistList& request, AudioCollectionResponse::ArtistList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		std::cerr << "No batch parameters found!" << std::endl;
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListArtists;
	size = std::min(size, _maxListArtists);

	// Now fetch requested data...

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Artist::pointer> artists = Artist::getAll( _db.getSession(), request.batch_parameter().offset(), static_cast<int>(size) );

	typedef Wt::Dbo::collection< Artist::pointer > Artists;

	for (Artists::const_iterator it = artists.begin(); it != artists.end(); ++it)
	{
		AudioCollectionResponse_Artist* artist = response.add_artists();

		artist->set_name((*it)->getName());
		artist->set_nb_releases(0);		// TODO
		artist->set_id(it->id());
	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetReleases(const AudioCollectionRequest::GetReleaseList& request, AudioCollectionResponse::ReleaseList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		std::cerr << "No batch parameters found!" << std::endl;
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListReleases;
	size = std::min(size, _maxListReleases);

	std::vector<Artist::id_type> artistIds;
	for (int id = 0; id < request.artist_id_size(); ++id)
		artistIds.push_back( request.artist_id(id) );

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Release::pointer> releases = Release::getAll( _db.getSession(), artistIds, request.batch_parameter().offset(), static_cast<int>(size));

	typedef Wt::Dbo::collection< Release::pointer > Releases;

	for (Releases::const_iterator it = releases.begin(); it != releases.end(); ++it)
	{
		AudioCollectionResponse_Release* release = response.add_releases();

		release->set_name((*it)->getName());
		release->set_id(it->id());
		// WARNING: next two lines are very time consuming!
		release->set_nb_tracks( (*it)->getTracks().size() );
		release->set_duration_secs( (*it)->getDuration().total_seconds() );
	}

	return true;
}

bool
AudioCollectionRequestHandler::processGetTracks(const AudioCollectionRequest::GetTrackList& request, AudioCollectionResponse::TrackList& response)
{
	// sanity checks
	if (!request.has_batch_parameter())
	{
		std::cerr << "No batch parameters found!" << std::endl;
		return false;
	}

	std::size_t  size = request.batch_parameter().size();
	if (!size)
		size = _maxListTracks;
	size = std::min(size, _maxListTracks);

	// Get filters
	std::vector<Artist::id_type> artistIds;
	for (int id = 0; id < request.artist_id_size(); ++id)
		artistIds.push_back( request.artist_id(id) );

	std::vector<Release::id_type> releaseIds;
	for (int id = 0; id < request.release_id_size(); ++id)
		releaseIds.push_back( request.release_id(id) );

	std::vector<Release::id_type> genreIds;
	for (int id = 0; id < request.genre_id_size(); ++id)
		genreIds.push_back( request.genre_id(id) );

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Track::pointer> tracks = Track::getAll( _db.getSession(),
							artistIds, releaseIds, genreIds,
							request.batch_parameter().offset(), static_cast<int>(size));

	typedef Wt::Dbo::collection< Track::pointer > Tracks;

	for (Tracks::const_iterator it = tracks.begin(); it != tracks.end(); ++it)
	{
		AudioCollectionResponse_Track* track = response.add_tracks();

		track->set_id(it->id());
		track->set_disc_number( (*it)->getDiscNumber() );
		track->set_track_number( (*it)->getTrackNumber() );
		track->set_artist_id( (*it)->getArtist().id() );
		track->set_release_id( (*it)->getRelease().id() );

		track->set_name( (*it)->getName() );
		track->set_duration_secs( (*it)->getDuration().total_seconds() );
//		if (!(*it)->getCreationTime().is_special())
//			track->set_release_date(  boost::posix_time::to_simple_string((*it)->getCreationTime()) );

		BOOST_FOREACH(Genre::pointer genre, (*it)->getGenres())
			track->add_genre_id( genre.id() );

//		if (!(*it)->getCoverArt().empty)
//			track->set_coverart( (*it)->getCoverArt() );
	}

	return true;
}

} // namespace Remote
} // namespace Server


