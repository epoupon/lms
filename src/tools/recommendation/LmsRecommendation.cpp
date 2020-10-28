/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#include <boost/program_options.hpp>

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "utils/IConfig.hpp"
#include "utils/Service.hpp"
#include "utils/StreamLogger.hpp"
#include "recommendation/IEngine.hpp"


static
void
dumpTracksRecommendation(Database::Session session, Recommendation::IEngine& engine)
{
	const std::vector<Database::IdType> trackIds {[&]()
		{
			auto transaction {session.createSharedTransaction()};
			return Database::Track::getAllIds(session);
		}()};

	std::cout << "*** Tracks (" << trackIds.size() << ") ***" << std::endl;
	for (Database::IdType trackId : trackIds)
	{
		auto trackToString = [&](Database::IdType trackId)
		{
			std::string res;
			auto transaction {session.createSharedTransaction()};
			Database::Track::pointer track {Database::Track::getById(session, trackId)};

			res += track->getName();
			if (track->getRelease())
				res += " [" + track->getRelease()->getName() + "]";
			for (auto artist : track->getArtists())
				res += " - " + artist->getName();
			for (auto cluster : track->getClusters())
				res += " {" + cluster->getType()->getName() + "-"+ cluster->getName() + "}";

			return res;
		};

		std::cout << "Processing track '" << trackToString(trackId) << std::endl;
		for (Database::IdType similarTrackId : engine.getSimilarTracks(session, {trackId}, 3))
			std::cout << "\t- Similar track '" << trackToString(similarTrackId) << std::endl;
	}
}

static
void
dumpReleasesRecommendation(Database::Session session, Recommendation::IEngine& engine)
{
	const std::vector<Database::IdType> releaseIds = std::invoke([&]()
			{
			auto transaction {session.createSharedTransaction()};
			return Database::Release::getAllIds(session);
			});

	std::cout << "*** Releases ***" << std::endl;
	for (Database::IdType releaseId : releaseIds)
	{
		auto releaseToString = [&](Database::IdType releaseId)
		{
			auto transaction {session.createSharedTransaction()};

			Database::Release::pointer release {Database::Release::getById(session, releaseId)};
			return release->getName();
		};

		std::cout << "Processing release '" << releaseToString(releaseId) << "'" << std::endl;
		for (Database::IdType similarReleaseId : engine.getSimilarReleases(session, releaseId, 3))
			std::cout << "\t- Similar release '" << releaseToString(similarReleaseId) << "'" << std::endl;
	}
}

static
void
dumpArtistsRecommendation(Database::Session session, Recommendation::IEngine& engine)
{
	const std::vector<Database::IdType> artistIds = std::invoke([&]()
			{
			auto transaction {session.createSharedTransaction()};
			return Database::Artist::getAllIds(session);
			});

	std::cout << "*** Artists ***" << std::endl;
	for (Database::IdType artistId : artistIds)
	{
		auto artistToString = [&](Database::IdType artistId)
		{
			auto transaction {session.createSharedTransaction()};

			Database::Artist::pointer artist {Database::Artist::getById(session, artistId)};
			return artist->getName();
		};

		std::cout << "Processing artist '" << artistToString(artistId) << "'" << std::endl;
		for (Database::IdType similarArtistId : engine.getSimilarArtists(session, artistId, 3))
			std::cout << "\t- Similar artist '" << artistToString(similarArtistId) << "'" << std::endl;
	}
}


int main(int argc, char *argv[])
{
	try
	{
		namespace po = boost::program_options;

		// log to stdout
		Service<Logger> logger {std::make_unique<StreamLogger>(std::cout)};

		po::options_description desc{"Allowed options"};
        desc.add_options()
        ("help,h", "print usage message")
		("conf,c", po::value<std::string>()->default_value("/etc/lms.conf"), "LMS config file")
        ("artists,a", "Display recommendation for artists")
        ("releases,r", "Display recommendation for releases")
        ("tracks,t", "Display recommendation for tracks")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
		{
			std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

		Service<IConfig> config {createConfig(vm["conf"].as<std::string>())};

		Database::Db db {config->getPath("working-dir") / "lms.db"};
		Database::Session session {db};

		std::cout << "Creating recommendation engine..." << std::endl;
		const auto engine {Recommendation::createEngine(db)};
		std::cout << "Recommendation engine created!" << std::endl;

		std::cout << "Loading recommendation engine..." << std::endl;
		engine->load(false);
		std::cout << "Recommendation engine loaded!" << std::endl;

		if (vm.count("tracks"))
			dumpTracksRecommendation(db, *engine);

		if (vm.count("releases"))
			dumpReleasesRecommendation(db, *engine);

		if (vm.count("artists"))
		dumpArtistsRecommendation(db, *engine);
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

