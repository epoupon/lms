#include "AudioCollectionRequestHandler.hpp"

#include "database/AudioTypes.hpp"

namespace Remote {
namespace Server {

AudioCollectionRequestHandler::AudioCollectionRequestHandler(DatabaseHandler& db)
: _db(db)
{}


bool
AudioCollectionRequestHandler::process(const AudioCollectionRequest& request, std::vector<ServerMessage> responses)
{

	switch (request.type())
	{
		case AudioCollectionRequest_Type_TypeGetArtistList:
			if (request.has_get_artists())
				return processGetArtists(request.get_artists(), responses);
			else
				std::cerr << "Bad AudioCollectionRequest_Type_TypeGetArtistList: message!" << std::endl;
			break;

		case AudioCollectionRequest_Type_TypeGetReleaseList:
			break;

		case AudioCollectionRequest_Type_TypeGetTrackList:
			break;

		default:
			std::cerr << "Unhandled AudioCollectionRequest_Type = " << request.type() << std::endl;
	}

	return false;
}

bool
AudioCollectionRequestHandler::processGetArtists(const AudioCollectionRequest::GetArtistList& request, std::vector<ServerMessage> responses)
{

	// sanity checks
	if (request.has_filter_name())
		std::cout << "Filter name = " << request.filter_name() << std::endl;

	for (int id = 0; id < request.filter_genre_size(); ++id)
	{
		std::cout << "Filter genre " << id << " = '" << request.filter_genre(id) << "'" << std::endl;
	}

	if (request.has_preferred_batch_size())
		std::cout << "Requested batch size = " << request.preferred_batch_size() << std::endl;


	// Now fetch requested data...

	std::cout << "Getting artists..." << std::endl;

	Wt::Dbo::Transaction transaction( _db.getSession() );

	Wt::Dbo::collection<Artist::pointer> artists = Artist::getAll( _db.getSession() );

	std::cout << "size = " << artists.size() << std::endl;

	typedef Wt::Dbo::collection< Artist::pointer > Artists;

	for (Artists::iterator it = artists.begin(); it != artists.end(); ++it)
	{
		std::cout << "Spotted artist = " << (*it)->getName() << std::endl;
	}

	std::cout << "Getting artists DONE" << std::endl;

	return false;
}

} // namespace Remote
} // namespace Server


