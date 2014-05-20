#include <algorithm>    // std::max

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
			break;

		case AudioCollectionRequest_Type_TypeGetTrackList:
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

	std::cout << "Offset = " << request.batch_parameter().offset() << std::endl;
	std::cout << "Size = " << request.batch_parameter().size() << std::endl;

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Genre::pointer> genres = Genre::getAll( _db.getSession(), request.batch_parameter().offset(), std::max(static_cast<std::size_t>(request.batch_parameter().size()), _maxListGenres) );

	typedef Wt::Dbo::collection< Genre::pointer > Genres;

	for (Genres::iterator it = genres.begin(); it != genres.end(); ++it)
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

	std::cout << "Offset = " << request.batch_parameter().offset() << std::endl;
	std::cout << "Size = " << request.batch_parameter().size() << std::endl;
	if (request.batch_parameter().size() > _maxListArtists)
		std::cerr << "Warning: batch parameter size too high (" << request.batch_parameter().size() << ")" << std::endl;


	for (int id = 0; id < request.genre_id_size(); ++id)
	{
		std::cout << "Genre id " << id << " = '" << request.genre_id(id) << "'" << std::endl;
	}


	// Now fetch requested data...

	std::cout << "Getting artists..." << std::endl;

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Artist::pointer> artists = Artist::getAll( _db.getSession(), request.batch_parameter().offset(), std::max(static_cast<std::size_t>(request.batch_parameter().size()), _maxListArtists) );

	std::cout << "size = " << artists.size() << std::endl;

	typedef Wt::Dbo::collection< Artist::pointer > Artists;

	for (Artists::iterator it = artists.begin(); it != artists.end(); ++it)
	{
		AudioCollectionResponse_Artist* artist = response.add_artists();

		artist->set_name((*it)->getName());
		artist->set_nb_releases(0);		// TODO
		artist->set_id(it->id());
	}

	std::cout << "Getting artists DONE" << std::endl;

	return true;
}

} // namespace Remote
} // namespace Server


