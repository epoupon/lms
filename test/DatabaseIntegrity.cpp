#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <boost/foreach.hpp>

#include "database/DatabaseHandler.hpp"

#include "database/AudioTypes.hpp"

int main(void)
{

	try {
		std::cout << "Starting test!" << std::endl;

		// Set up the long living database session
		DatabaseHandler database("test.db");

		Wt::Dbo::Transaction transaction(database.getSession());

		Wt::Dbo::collection< Track::pointer > tracks (Track::getAll( database.getSession() ));

		std::cout << "Found " << tracks.size() << " tracks!" << std::endl;

		BOOST_FOREACH(Track::pointer track, tracks ) {
			assert( !track->getName().empty() );
			assert( track->getArtist() );
			assert( track->getRelease() );
			assert( !track->getGenres().empty() );
			assert( !track->getDuration().is_not_a_date_time()  );
		}

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
