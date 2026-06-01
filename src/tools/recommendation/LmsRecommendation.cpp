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

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
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
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "services/recommendation/IRecommendationService.hpp"

namespace lms
{
    void dumpTracksRecommendation(db::Session session, recommendation::IRecommendationService& recommendationService, std::string_view name, unsigned maxCount)
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

            for (const auto& similarTrack : recommendationService.findSimilarTracks(std::span<const db::TrackId>{ &trackId, 1 }, maxCount))
                std::cout << "\t- " << similarTrack.distance << ", Similar track " << trackToString(similarTrack.id) << std::endl;
        }
    }

    void dumpReleasesRecommendation(db::Session session, recommendation::IRecommendationService& recommendationService, std::string_view name, unsigned maxCount)
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
            for (const auto& similarRelease : recommendationService.findSimilarReleases(releaseId, maxCount))
                std::cout << "\t- " << similarRelease.distance << ", Similar release " << releaseToString(similarRelease.id) << std::endl;
        }
    }

    void dumpArtistsRecommendation(db::Session session, recommendation::IRecommendationService& recommendationService, std::string_view name, unsigned maxCount)
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
            for (const auto& similarArtist : recommendationService.findSimilarArtists(artistId, { db::TrackArtistLinkType::Artist }, maxCount))
                std::cout << "\t- " << similarArtist.distance << ", Similar artist '" << artistToString(similarArtist.id) << "'" << std::endl;
        }
    }

    void dumpRandomTracksRecommendation(
        db::Session session,
        recommendation::IRecommendationService& recommendationService,
        std::size_t randomTrackCount,
        unsigned seed,
        unsigned maxCount)
    {
        if (randomTrackCount == 0)
            return;

        std::vector<db::TrackId> trackIds;
        {
            auto transaction{ session.createReadTransaction() };
            trackIds.reserve(db::TrackMusicNNEmbeddings::getCount(session));
            db::TrackMusicNNEmbeddings::find(session, [&](const db::TrackMusicNNEmbeddings::pointer& features) {
                trackIds.push_back(features->getTrackId());
            });
        }

        if (trackIds.empty())
        {
            std::cout << "No tracks with TrackMusicNNEmbeddings found" << std::endl;
            return;
        }

        const unsigned effectiveSeed{ seed != 0 ? seed : std::random_device{}() };
        std::minstd_rand rng{ effectiveSeed };
        std::shuffle(std::begin(trackIds), std::end(trackIds), rng);

        const std::size_t queryCount{ std::min(randomTrackCount, trackIds.size()) };

        auto trackToString = [&](const db::TrackId trackId) {
            auto transaction{ session.createReadTransaction() };
            const db::Track::pointer track{ db::Track::find(session, trackId) };

            std::ostringstream oss;
            oss << "'" << track->getName() << "'";
            if (track->getRelease())
                oss << " [" << track->getRelease()->getName() << "]";
            if (std::string_view artistDisplayName{ track->getArtistDisplayName() }; !artistDisplayName.empty())
                oss << " by '" << artistDisplayName << "'";

            return oss.str();
        };

        std::cout << "*** Random Tracks Baseline ***" << std::endl;
        std::cout << "seed=" << effectiveSeed << ", count=" << queryCount << ", max=" << maxCount << std::endl;

        for (std::size_t i{}; i < queryCount; ++i)
        {
            const db::TrackId trackId{ trackIds[i] };

            std::cout << "Query " << (i + 1) << ": " << trackToString(trackId) << std::endl;
            const auto similarTracks{ recommendationService.findSimilarTracks(std::span<const db::TrackId>{ &trackId, 1 }, maxCount) };

            if (similarTracks.empty())
            {
                std::cout << "\t- no similar tracks" << std::endl;
                continue;
            }

            for (const auto& similarTrack : similarTracks)
                std::cout << "\t- " << similarTrack.distance << ", Similar track " << trackToString(similarTrack.id) << std::endl;
        }
    }

    void dumpRandomReleasesRecommendation(
        db::Session session,
        recommendation::IRecommendationService& recommendationService,
        std::size_t randomReleaseCount,
        unsigned seed,
        unsigned maxCount)
    {
        if (randomReleaseCount == 0)
            return;

        std::vector<db::ReleaseId> releaseIds;
        {
            auto transaction{ session.createReadTransaction() };
            releaseIds = db::Release::findIds(session, db::Release::FindParameters{}).results;
        }

        if (releaseIds.empty())
        {
            std::cout << "No releases found" << std::endl;
            return;
        }

        const unsigned effectiveSeed{ seed != 0 ? seed : std::random_device{}() };
        std::minstd_rand rng{ effectiveSeed };
        std::shuffle(std::begin(releaseIds), std::end(releaseIds), rng);

        const std::size_t queryCount{ std::min(randomReleaseCount, releaseIds.size()) };

        auto releaseToString = [&](const db::ReleaseId releaseId) -> std::string {
            auto transaction{ session.createReadTransaction() };
            const db::Release::pointer release{ db::Release::find(session, releaseId) };

            std::ostringstream oss;
            oss << "'" << release->getName() << "'";
            if (std::string_view artistDisplayName{ release->getArtistDisplayName() }; !artistDisplayName.empty())
                oss << " by '" << artistDisplayName << "'";

            return oss.str();
        };

        std::cout << "*** Random Releases Baseline ***" << std::endl;
        std::cout << "seed=" << effectiveSeed << ", count=" << queryCount << ", max=" << maxCount << std::endl;

        for (std::size_t i{}; i < queryCount; ++i)
        {
            const db::ReleaseId releaseId{ releaseIds[i] };

            std::cout << "Query " << (i + 1) << ": " << releaseToString(releaseId) << std::endl;
            const auto similarReleases{ recommendationService.findSimilarReleases(releaseId, maxCount) };

            if (similarReleases.empty())
            {
                std::cout << "\t- no similar releases" << std::endl;
                continue;
            }

            for (const auto& similarRelease : similarReleases)
                std::cout << "\t- " << similarRelease.distance << ", Similar release " << releaseToString(similarRelease.id) << std::endl;
        }
    }

    void dumpRandomArtistsRecommendation(
        db::Session session,
        recommendation::IRecommendationService& recommendationService,
        std::size_t randomArtistCount,
        unsigned seed,
        unsigned maxCount)
    {
        if (randomArtistCount == 0)
            return;

        std::vector<db::ArtistId> artistIds;
        {
            auto transaction{ session.createReadTransaction() };
            artistIds = db::Artist::findIds(session, db::Artist::FindParameters{}).results;
        }

        if (artistIds.empty())
        {
            std::cout << "No artists found" << std::endl;
            return;
        }

        const unsigned effectiveSeed{ seed != 0 ? seed : std::random_device{}() };
        std::minstd_rand rng{ effectiveSeed };
        std::shuffle(std::begin(artistIds), std::end(artistIds), rng);

        const std::size_t queryCount{ std::min(randomArtistCount, artistIds.size()) };

        auto artistToString = [&](const db::ArtistId artistId) {
            auto transaction{ session.createReadTransaction() };
            const db::Artist::pointer artist{ db::Artist::find(session, artistId) };
            return artist->getName();
        };

        std::cout << "*** Random Artists Baseline ***" << std::endl;
        std::cout << "seed=" << effectiveSeed << ", count=" << queryCount << ", max=" << maxCount << std::endl;

        for (std::size_t i{}; i < queryCount; ++i)
        {
            const db::ArtistId artistId{ artistIds[i] };

            std::cout << "Query " << (i + 1) << ": '" << artistToString(artistId) << "'" << std::endl;
            const auto similarArtists{ recommendationService.findSimilarArtists(artistId, { db::TrackArtistLinkType::Artist }, maxCount) };

            if (similarArtists.empty())
            {
                std::cout << "\t- no similar artists" << std::endl;
                continue;
            }

            for (const auto& similarArtist : similarArtists)
                std::cout << "\t- " << similarArtist.distance << ", Similar artist '" << artistToString(similarArtist.id) << "'" << std::endl;
        }
    }

    void dumpTrackPaths(
        db::Session session,
        recommendation::IRecommendationService& recommendationService,
        std::string_view fromName,
        std::string_view toName,
        unsigned maxCount)
    {
        std::vector<db::TrackId> fromTrackIds;
        std::vector<db::TrackId> toTrackIds;

        // Find 'from' tracks
        if (const auto mbid{ core::UUID::fromString(fromName) })
        {
            auto transaction{ session.createReadTransaction() };
            for (const auto& track : db::Track::findByMBID(session, *mbid))
                fromTrackIds.push_back(track->getId());
        }
        else
        {
            db::Track::FindParameters params;
            params.setKeywords(core::stringUtils::splitString(fromName, ' '));

            auto transaction{ session.createReadTransaction() };
            fromTrackIds = db::Track::findIds(session, params).results;
        }

        // Find 'to' tracks
        if (const auto mbid{ core::UUID::fromString(toName) })
        {
            auto transaction{ session.createReadTransaction() };
            for (const auto& track : db::Track::findByMBID(session, *mbid))
                toTrackIds.push_back(track->getId());
        }
        else
        {
            db::Track::FindParameters params;
            params.setKeywords(core::stringUtils::splitString(toName, ' '));

            auto transaction{ session.createReadTransaction() };
            toTrackIds = db::Track::findIds(session, params).results;
        }

        if (fromTrackIds.empty() || toTrackIds.empty())
        {
            std::cout << "*** Track Paths ***" << std::endl;
            std::cout << "No matching tracks found" << std::endl;
            return;
        }

        auto trackToString = [&](const db::TrackId trackId) {
            auto transaction{ session.createReadTransaction() };
            const db::Track::pointer track{ db::Track::find(session, trackId) };

            std::ostringstream oss;
            oss << "'" << track->getName() << "'";
            if (track->getRelease())
                oss << " [" << track->getRelease()->getName() << "]";
            if (std::string_view artistDisplayName{ track->getArtistDisplayName() }; !artistDisplayName.empty())
                oss << " by '" << artistDisplayName << "'";

            return oss.str();
        };

        std::cout << "*** Track Paths ***" << std::endl;
        std::cout << "From (" << fromTrackIds.size() << ") To (" << toTrackIds.size() << ") - max path length " << maxCount << std::endl;

        std::size_t pathCount{};
        for (const auto& fromTrackId : fromTrackIds)
        {
            for (const auto& toTrackId : toTrackIds)
            {
                ++pathCount;
                std::cout << std::endl
                          << "Path " << pathCount << ": ";
                std::cout << trackToString(fromTrackId) << " => " << trackToString(toTrackId) << std::endl;

                const auto path{ recommendationService.findTrackSimilarityPath(fromTrackId, toTrackId, maxCount) };

                if (path.empty())
                {
                    std::cout << "\t(no path found)" << std::endl;
                    continue;
                }

                for (std::size_t i{}; i < path.size(); ++i)
                {
                    const auto& result{ path[i] };
                    std::cout << "\t" << (i + 1) << ". " << trackToString(result.id) << " (distance: " << result.distance << ")" << std::endl;
                }
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
        core::Service<core::logging::ILogger> logger{ core::logging::createLogger(core::logging::Severity::DEBUG) };

        po::options_description desc{ "Allowed options" };
        // clang-format off
        desc.add_options()
            ("help,h", "print usage message")
            ("conf,c", po::value<std::string>()->default_value(core::sysconfDirectory / "lms.conf"), "LMS config file")
            ("artist,a", po::value<std::string>(), "Display recommendation for a given artist (mbid or name search pattern)")
            ("release,r", po::value<std::string>(), "Display recommendation for releases (mbid or name search pattern)")
            ("track,t", po::value<std::string>(), "Display recommendation for tracks (track mbid or name search pattern)")
            ("track-path-from", po::value<std::string>(), "Find similarity path from track (mbid or name search pattern)")
            ("track-path-to", po::value<std::string>(), "Find similarity path to track (mbid or name search pattern)")
            ("random-tracks", po::value<unsigned>(), "Display recommendation for N random tracks")
            ("random-releases", po::value<unsigned>(), "Display recommendation for N random releases")
            ("random-artists", po::value<unsigned>(), "Display recommendation for N random artists")
            ("seed", po::value<unsigned>()->default_value(0), "Seed used with --random-tracks/--random-releases/--random-artists (0 means random seed)")
            ("max,m", po::value<unsigned>()->default_value(10), "Max recommendation result count");
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

        if (recommendationService->getEngineType() == recommendation::EngineType::None)
        {
            std::cout << "Recommendation engine is disabled" << std::endl;
            return EXIT_SUCCESS;
        }

        while (!recommendationService->isLoaded())
            std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });

        unsigned maxCount{ vm["max"].as<unsigned>() };

        if (vm.count("track"))
            dumpTracksRecommendation(*db, *recommendationService, vm["track"].as<std::string>(), maxCount);

        if (vm.count("release"))
            dumpReleasesRecommendation(*db, *recommendationService, vm["release"].as<std::string>(), maxCount);

        if (vm.count("artist"))
            dumpArtistsRecommendation(*db, *recommendationService, vm["artist"].as<std::string>(), maxCount);

        if (vm.count("track-path-from") && vm.count("track-path-to"))
            dumpTrackPaths(*db, *recommendationService, vm["track-path-from"].as<std::string>(), vm["track-path-to"].as<std::string>(), maxCount);

        if (vm.count("random-tracks"))
            dumpRandomTracksRecommendation(*db, *recommendationService, vm["random-tracks"].as<unsigned>(), vm["seed"].as<unsigned>(), maxCount);

        if (vm.count("random-releases"))
            dumpRandomReleasesRecommendation(*db, *recommendationService, vm["random-releases"].as<unsigned>(), vm["seed"].as<unsigned>(), maxCount);

        if (vm.count("random-artists"))
            dumpRandomArtistsRecommendation(*db, *recommendationService, vm["random-artists"].as<unsigned>(), vm["seed"].as<unsigned>(), maxCount);
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
