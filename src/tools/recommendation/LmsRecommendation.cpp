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

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

#include <boost/program_options.hpp>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "core/SystemPaths.hpp"
#include "core/UUID.hpp"
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
    void dumpTracksRecommendation(db::Session session, recommendation::IRecommendationService& recommendationService, std::string_view name, unsigned maxSimilarityCount)
    {
        std::vector<db::TrackId> trackIds;

        if (const auto mbid{ core::UUID::fromString(name) })
        {
            auto transaction{ session.createReadTransaction() };
            for (const auto& track : db::Track::findByMBID(session, *mbid))
                trackIds.push_back(track->getId());
        }
        else
        {
            db::Track::FindParameters params;
            params.setKeywords(core::stringUtils::splitString(name, ' '));

            auto transaction{ session.createReadTransaction() };
            trackIds = db::Track::findIds(session, params).results;
        }

        std::cout << "*** Tracks (" << trackIds.size() << ") ***" << std::endl;
        for (const db::TrackId trackId : trackIds)
        {
            auto trackToString = [&](const db::TrackId trackId) {
                auto transaction{ session.createReadTransaction() };
                const db::Track::pointer track{ db::Track::find(session, trackId) };

                std::ostringstream oss;
                oss << "'" << track->getName() << "'";
                if (track->getRelease())
                    oss << " [" << track->getRelease()->getName() << "]";
                if (std::string_view artistDisplayName{ track->getArtistDisplayName() }; !artistDisplayName.empty())
                    oss << " by '" << artistDisplayName << "'";
                for (const auto& cluster : track->getClusters())
                    oss << " {" << cluster->getType()->getName() << "-" << cluster->getName() << "}";
                oss << " - '" + track->getAbsoluteFilePath().string() << "'";

                return oss.str();
            };

            std::cout << "Processing track " << trackToString(trackId) << std::endl;
            for (db::TrackId similarTrackId : recommendationService.findSimilarTracks({ trackId }, maxSimilarityCount))
                std::cout << "\t- Similar track " << trackToString(similarTrackId) << std::endl;
        }
    }

    void dumpReleasesRecommendation(db::Session session, recommendation::IRecommendationService& recommendationService, std::string_view name, unsigned maxSimilarityCount)
    {
        std::vector<db::ReleaseId> releaseIds;

        if (const auto mbid{ core::UUID::fromString(name) })
        {
            auto transaction{ session.createReadTransaction() };
            if (const auto release{ db::Release::find(session, *mbid) })
                releaseIds.push_back(release->getId());
        }
        else
        {
            db::Release::FindParameters params;
            params.setKeywords(core::stringUtils::splitString(name, ' '));

            auto transaction{ session.createReadTransaction() };
            releaseIds = db::Release::findIds(session, params).results;
        }

        std::cout << "*** Releases ***" << std::endl;
        for (const db::ReleaseId releaseId : releaseIds)
        {
            auto releaseToString = [&](db::ReleaseId releaseId) -> std::string {
                auto transaction{ session.createReadTransaction() };

                const db::Release::pointer release{ db::Release::find(session, releaseId) };

                std::ostringstream oss;

                oss << "'" << release->getName() << "'";
                if (std::string_view artistDisplayName{ release->getArtistDisplayName() }; !artistDisplayName.empty())
                    oss << " by '" << artistDisplayName << "'";

                return oss.str();
            };

            std::cout << "Processing release '" << releaseToString(releaseId) << "'" << std::endl;
            for (const db::ReleaseId similarReleaseId : recommendationService.getSimilarReleases(releaseId, maxSimilarityCount))
                std::cout << "\t- Similar release " << releaseToString(similarReleaseId) << std::endl;
        }
    }

    void dumpArtistsRecommendation(db::Session session, recommendation::IRecommendationService& recommendationService, std::string_view name, unsigned maxSimilarityCount)
    {
        std::vector<db::ArtistId> artistIds;

        if (const auto mbid{ core::UUID::fromString(name) })
        {
            auto transaction{ session.createReadTransaction() };
            if (const auto artist{ db::Artist::find(session, *mbid) })
                artistIds.push_back(artist->getId());
        }
        else
        {
            db::Artist::FindParameters params;
            params.setKeywords(core::stringUtils::splitString(name, ' '));

            auto transaction{ session.createReadTransaction() };
            artistIds = db::Artist::findIds(session, params).results;
        }

        std::cout << "*** Artists ***" << std::endl;
        for (db::ArtistId artistId : artistIds)
        {
            auto artistToString = [&](db::ArtistId artistId) {
                auto transaction{ session.createReadTransaction() };

                db::Artist::pointer artist{ db::Artist::find(session, artistId) };
                return artist->getName();
            };

            std::cout << "Processing artist '" << artistToString(artistId) << "'" << std::endl;
            for (db::ArtistId similarArtistId : recommendationService.getSimilarArtists(artistId, { db::TrackArtistLinkType::Artist }, maxSimilarityCount))
                std::cout << "\t- Similar artist '" << artistToString(similarArtistId) << "'" << std::endl;
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
        core::Service<core::logging::ILogger> logger{ core::logging::createLogger(core::logging::Severity::DEBUG) };

        po::options_description desc{ "Allowed options" };
        // clang-format off
        desc.add_options()
            ("help,h", "print usage message")
            ("conf,c", po::value<std::string>()->default_value(core::sysconfDirectory / "lms.conf"), "LMS config file")
            ("artist,a", po::value<std::string>(), "Display recommendation for a given artist (mbid or name search pattern)")
            ("release,r", po::value<std::string>(), "Display recommendation for releases (mbid or name search pattern)")
            ("track,t", po::value<std::string>(), "Display recommendation for tracks (track mbid or name search pattern)")
            ("max,m", po::value<unsigned>()->default_value(10), "Max similarity result count");
        // clang-format on

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return EXIT_SUCCESS;
        }

        core::Service<core::IConfig> config{ core::createConfig(vm["conf"].as<std::string>()) };

        auto db{ db::createDb(config->getPath("working-dir", "/var/lms") / "lms.db") };
        db::Session session{ *db };

        const auto recommendationService{ recommendation::createRecommendationService(*db) };

        unsigned maxSimilarityCount{ vm["max"].as<unsigned>() };

        // TODO change this
        std::this_thread::sleep_for(std::chrono::seconds{ 5 });

        if (vm.count("track"))
            dumpTracksRecommendation(*db, *recommendationService, vm["track"].as<std::string>(), maxSimilarityCount);

        if (vm.count("release"))
            dumpReleasesRecommendation(*db, *recommendationService, vm["release"].as<std::string>(), maxSimilarityCount);

        if (vm.count("artist"))
            dumpArtistsRecommendation(*db, *recommendationService, vm["artist"].as<std::string>(), maxSimilarityCount);
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
