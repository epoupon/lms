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
#include <stdlib.h>

#include <boost/program_options.hpp>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "core/SystemPaths.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "services/recommendation/IRecommendationService.hpp"

namespace lms
{
    using namespace db;

    void dumpTracksRecommendation(Session session, recommendation::IRecommendationService& recommendationService, unsigned maxSimilarityCount)
    {
        const RangeResults<TrackId> trackIds{ [&] {
            auto transaction{ session.createReadTransaction() };
            return Track::findIds(session, Track::FindParameters{});
        }() };

        std::cout << "*** Tracks (" << trackIds.results.size() << ") ***" << std::endl;
        for (const TrackId trackId : trackIds.results)
        {
            auto trackToString = [&](const TrackId trackId) {
                std::string res;
                auto transaction{ session.createReadTransaction() };
                const Track::pointer track{ Track::find(session, trackId) };

                res += track->getName();
                if (track->getRelease())
                    res += " [" + std::string{ track->getRelease()->getName() } + "]";
                for (const auto& artist : track->getArtists({ TrackArtistLinkType::Artist }))
                    res += " - " + artist->getName();
                for (const auto& cluster : track->getClusters())
                    res += " {" + std::string{ cluster->getType()->getName() } + "-" + std::string{ cluster->getName() } + "}";

                return res;
            };

            std::cout << "Processing track '" << trackToString(trackId) << std::endl;
            for (TrackId similarTrackId : recommendationService.findSimilarTracks({ trackId }, maxSimilarityCount))
                std::cout << "\t- Similar track '" << trackToString(similarTrackId) << std::endl;
        }
    }

    void dumpReleasesRecommendation(Session session, recommendation::IRecommendationService& recommendationService, unsigned maxSimilarityCount)
    {
        const RangeResults<ReleaseId> releaseIds{ std::invoke([&] {
            auto transaction{ session.createReadTransaction() };
            return Release::findIds(session, Release::FindParameters{});
        }) };

        std::cout << "*** Releases ***" << std::endl;
        for (const ReleaseId releaseId : releaseIds.results)
        {
            auto releaseToString = [&](ReleaseId releaseId) -> std::string {
                auto transaction{ session.createReadTransaction() };

                Release::pointer release{ Release::find(session, releaseId) };
                return std::string{ release->getName() };
            };

            std::cout << "Processing release '" << releaseToString(releaseId) << "'" << std::endl;
            for (const ReleaseId similarReleaseId : recommendationService.getSimilarReleases(releaseId, maxSimilarityCount))
                std::cout << "\t- Similar release '" << releaseToString(similarReleaseId) << "'" << std::endl;
        }
    }

    void dumpArtistsRecommendation(Session session, recommendation::IRecommendationService& recommendationService, unsigned maxSimilarityCount)
    {
        const RangeResults<ArtistId> artistIds = std::invoke([&]() {
            auto transaction{ session.createReadTransaction() };
            return Artist::findIds(session, Artist::FindParameters{});
        });

        std::cout << "*** Artists ***" << std::endl;
        for (ArtistId artistId : artistIds.results)
        {
            auto artistToString = [&](ArtistId artistId) {
                auto transaction{ session.createReadTransaction() };

                Artist::pointer artist{ Artist::find(session, artistId) };
                return artist->getName();
            };

            std::cout << "Processing artist '" << artistToString(artistId) << "'" << std::endl;
            for (ArtistId similarArtistId : recommendationService.getSimilarArtists(artistId, { TrackArtistLinkType::Artist, TrackArtistLinkType::ReleaseArtist }, maxSimilarityCount))
            {
                std::cout << "\t- Similar artist '" << artistToString(similarArtistId) << "'" << std::endl;
            }
        }
    }
} // namespace lms

int main(int argc, char* argv[])
{
    try
    {
        using namespace lms;
        namespace po = boost::program_options;

        // log to stdout
        core::Service<core::logging::ILogger> logger{ core::logging::createLogger() };

        po::options_description desc{ "Allowed options" };
        desc.add_options()("help,h", "print usage message")("conf,c", po::value<std::string>()->default_value(core::sysconfDirectory / "lms.conf"), "LMS config file")("artists,a", "Display recommendation for artists")("releases,r", "Display recommendation for releases")("tracks,t", "Display recommendation for tracks")("max,m", po::value<unsigned>()->default_value(3), "Max similarity result count");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

        core::Service<core::IConfig> config{ core::createConfig(vm["conf"].as<std::string>()) };

        auto db{ db::createDb(config->getPath("working-dir", "/var/lms") / "lms.db") };
        Session session{ *db };

        std::cout << "Creating recommendation service..." << std::endl;
        const auto recommendationService{ recommendation::createRecommendationService(*db) };
        std::cout << "Recommendation service created!" << std::endl;

        std::cout << "Loading recommendation service..." << std::endl;
        recommendationService->load();

        unsigned maxSimilarityCount{ vm["max"].as<unsigned>() };

        std::cout << "Recommendation service loaded!" << std::endl;

        if (vm.count("tracks"))
            dumpTracksRecommendation(*db, *recommendationService, maxSimilarityCount);

        if (vm.count("releases"))
            dumpReleasesRecommendation(*db, *recommendationService, maxSimilarityCount);

        if (vm.count("artists"))
            dumpArtistsRecommendation(*db, *recommendationService, maxSimilarityCount);
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
