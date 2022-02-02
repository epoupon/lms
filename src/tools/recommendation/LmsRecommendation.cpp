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

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/Types.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "utils/IConfig.hpp"
#include "utils/Service.hpp"
#include "utils/StreamLogger.hpp"

using namespace Database;

static
void
dumpTracksRecommendation(Session session, Recommendation::IRecommendationService& recommendationService, unsigned maxSimilarityCount)
{
	const RangeResults<TrackId> trackIds {[&]()
		{
			auto transaction {session.createSharedTransaction()};
			return Track::find(session, Track::FindParameters {});
		}()};

	std::cout << "*** Tracks (" << trackIds.results.size() << ") ***" << std::endl;
	for (const TrackId trackId : trackIds.results)
	{
		auto trackToString = [&](const TrackId trackId)
		{
			std::string res;
			auto transaction {session.createSharedTransaction()};
			const Track::pointer track {Track::find(session, trackId)};

			res += track->getName();
			if (track->getRelease())
				res += " [" + track->getRelease()->getName() + "]";
			for (auto artist : track->getArtists({TrackArtistLinkType::Artist}))
				res += " - " + artist->getName();
			for (auto cluster : track->getClusters())
				res += " {" + cluster->getType()->getName() + "-"+ cluster->getName() + "}";

			return res;
		};

		std::cout << "Processing track '" << trackToString(trackId) << std::endl;
		for (TrackId similarTrackId : recommendationService.findSimilarTracks({trackId}, maxSimilarityCount))
			std::cout << "\t- Similar track '" << trackToString(similarTrackId) << std::endl;
	}
}

static
void
dumpReleasesRecommendation(Session session, Recommendation::IRecommendationService& recommendationService, unsigned maxSimilarityCount)
{
	const RangeResults<ReleaseId> releaseIds {std::invoke([&]()
		{
			auto transaction {session.createSharedTransaction()};
			return Release::find(session, Release::FindParameters {});
		})};

	std::cout << "*** Releases ***" << std::endl;
	for (ReleaseId releaseId : releaseIds.results)
	{
		auto releaseToString = [&](ReleaseId releaseId) -> std::string
		{
			auto transaction {session.createSharedTransaction()};

			Release::pointer release {Release::find(session, releaseId)};
			return release->getName();
		};

		std::cout << "Processing release '" << releaseToString(releaseId) << "'" << std::endl;
		for (ReleaseId similarReleaseId : recommendationService.getSimilarReleases(releaseId, maxSimilarityCount))
			std::cout << "\t- Similar release '" << releaseToString(similarReleaseId) << "'" << std::endl;
	}
}

static
void
dumpArtistsRecommendation(Session session, Recommendation::IRecommendationService& recommendationService, unsigned maxSimilarityCount)
{
	const RangeResults<ArtistId> artistIds = std::invoke([&]()
	{
		auto transaction {session.createSharedTransaction()};
		return Artist::find(session, Artist::FindParameters {});
	});

	std::cout << "*** Artists ***" << std::endl;
	for (ArtistId artistId : artistIds.results)
	{
		auto artistToString = [&](ArtistId artistId)
		{
			auto transaction {session.createSharedTransaction()};

			Artist::pointer artist {Artist::find(session, artistId)};
			return artist->getName();
		};

		std::cout << "Processing artist '" << artistToString(artistId) << "'" << std::endl;
		for (ArtistId similarArtistId : recommendationService.getSimilarArtists(artistId, {TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist}, maxSimilarityCount))
		{
			std::cout << "\t- Similar artist '" << artistToString(similarArtistId) << "'" << std::endl;
		}
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
		("max,m", po::value<unsigned>()->default_value(3), "Max similarity result count")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
		{
			std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

		Service<IConfig> config {createConfig(vm["conf"].as<std::string>())};

		Db db {config->getPath("working-dir") / "lms.db"};
		Session session {db};

		std::cout << "Creating recommendation recommendationService..." << std::endl;
		const auto recommendationService {Recommendation::createRecommendationService(db)};
		std::cout << "Recommendation recommendationService created!" << std::endl;

		std::cout << "Loading recommendation recommendationService..." << std::endl;
		recommendationService->load(false);

		unsigned maxSimilarityCount {vm["max"].as<unsigned>()};

		std::cout << "Recommendation recommendationService loaded!" << std::endl;

		if (vm.count("tracks"))
			dumpTracksRecommendation(db, *recommendationService, maxSimilarityCount);

		if (vm.count("releases"))
			dumpReleasesRecommendation(db, *recommendationService, maxSimilarityCount);

		if (vm.count("artists"))
			dumpArtistsRecommendation(db, *recommendationService, maxSimilarityCount);
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

