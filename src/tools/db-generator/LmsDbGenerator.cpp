/*
 * Copyright (C) 2023 Emeric Poupon
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
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <stdlib.h>
#include <string>

#include <boost/program_options.hpp>

#include "services/database/Db.hpp"
#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackArtistLink.hpp"

#include "utils/IConfig.hpp"
#include "utils/Random.hpp"
#include "utils/StreamLogger.hpp"
#include "utils/Service.hpp"

namespace
{
    struct GeneratorParameters
    {
        std::size_t releaseCountPerBatch{ 1000 };
        std::size_t releaseCount{ 100 };
        std::size_t trackCountPerRelease{ 10 };
        float compilationRatio{ 0.1 };
        std::size_t genreCountPerTrack{ 3 };
        std::size_t moodCountPerTrack{ 3 };
        std::size_t genreCount{ 50 };
        std::size_t moodCount{ 25 };
        std::filesystem::path trackPath;
    };

    struct GenerationContext
    {
        Database::Session& session;
        std::vector<Database::Cluster::pointer> genres;
        std::vector<Database::Cluster::pointer> moods;
        GenerationContext(Database::Session& _session) : session{ _session } {}
    };

    Database::Cluster::pointer generateCluster(Database::Session& session, Database::ClusterType::pointer clusterType)
    {
        const std::string clusterName{ clusterType->getName() + "-" + std::string{ UUID::generate().getAsString() } };
        return session.create<Database::Cluster>(clusterType, clusterName);
    }

    Database::Artist::pointer generateArtist(Database::Session& session)
    {
        const UUID artistMBID{ UUID::generate() };
        const std::string artistName{ "Artist-" + std::string{ UUID::generate().getAsString() } };
        return session.create<Database::Artist>(artistName, artistMBID);
    }

    void generateRelease(const GeneratorParameters& params, GenerationContext& context)
    {
        using namespace Database;

        const UUID releaseMBID{ UUID::generate() };
        const std::string releaseName{ "Release-" + std::string{ UUID::generate().getAsString() } };
        Release::pointer release{ context.session.create<Release>(releaseName, releaseMBID) };

        Artist::pointer artist{ generateArtist(context.session) };

        for (std::size_t i{}; i < params.trackCountPerRelease; ++i)
        {
            Track::pointer track{ context.session.create<Track>(params.trackPath) };

            track.modify()->setName("Track-" + std::string{ UUID::generate().getAsString() });
            track.modify()->setDiscNumber(1);
            track.modify()->setTrackNumber(i);
            track.modify()->setDuration(std::chrono::seconds{ Random::getRandom(30, 300) });
            track.modify()->setRelease(release);
            track.modify()->setTrackMBID(UUID::generate());
            track.modify()->setRecordingMBID(UUID::generate());
            track.modify()->setTotalTrack(params.trackCountPerRelease);

            TrackArtistLink::create(context.session, track, artist, TrackArtistLinkType::Artist);
            TrackArtistLink::create(context.session, track, artist, TrackArtistLinkType::ReleaseArtist);

            std::vector<ObjectPtr<Cluster>> clusters;
            clusters.push_back(*Random::pickRandom(context.genres));
            clusters.push_back(*Random::pickRandom(context.moods));
            track.modify()->setClusters(clusters);
        }
    }

    void generate(const GeneratorParameters& params, GenerationContext& context)
    {
        std::size_t remainingCount{ params.releaseCount };

        while(remainingCount > 0)
        {
            auto transaction{ context.session.createUniqueTransaction() };
            std::cout << "Generating album #" << params.releaseCount - remainingCount << " / " << params.releaseCount << std::endl;

            for (std::size_t i{}; i < params.releaseCountPerBatch && remainingCount-- > 0; ++i)
                generateRelease(params, context);
        }
    }

    void prepareContext(const GeneratorParameters& params, GenerationContext& context)
    {
        auto transaction{ context.session.createUniqueTransaction() };

        // create some random genres/moods
        {
            Database::ClusterType::pointer genre{ Database::ClusterType::find(context.session, "GENRE") };
            if (!genre)
                genre = context.session.create<Database::ClusterType>("GENRE");

            for (std::size_t i{}; i < params.genreCount; ++i)
                context.genres.push_back(generateCluster(context.session, genre));
        }

        {
            Database::ClusterType::pointer mood{ Database::ClusterType::find(context.session, "MOOD") };
            if (!mood)
                mood = context.session.create<Database::ClusterType>("MOOD");

            for (std::size_t i{}; i < params.moodCount; ++i)
                context.moods.push_back(generateCluster(context.session, mood));
        }
    }
}

int main(int argc, char* argv[])
{
    try
    {
        // log to stdout
        Service<Logger> logger{ std::make_unique<StreamLogger>(std::cout) };

        namespace po = boost::program_options;

        const GeneratorParameters defaultParams;

        po::options_description options{ "Options" };
        options.add_options()
            ("conf,c", po::value<std::string>()->default_value("/etc/lms.conf"), "lms config file")
            ("release-count-per-batch", po::value<unsigned>()->default_value(defaultParams.releaseCountPerBatch), "Number of releases to generate before committing transaction")
            ("release-count", po::value<unsigned>()->default_value(defaultParams.releaseCount), "Number of releases to generate")
            ("track-count-per-release", po::value<unsigned>()->default_value(defaultParams.trackCountPerRelease), "Number of tracks per release")
            ("compilation-ratio", po::value<float>()->default_value(defaultParams.compilationRatio), "Compilation ratio (compilation means all tracks have a different artist)")
            ("track-path", po::value<std::string>()->required(), "Path of a valid track file, that will be used for all generated tracks")
            ("genre-count", po::value<unsigned>()->default_value(defaultParams.genreCount), "Number of genres to generate")
            ("genre-count-per-track", po::value<unsigned>()->default_value(defaultParams.genreCountPerTrack), "Number of genres to assign to each track")
            ("mood-count", po::value<unsigned>()->default_value(defaultParams.moodCount), "Number of moods to generate")
            ("mood-count-per-track", po::value<unsigned>()->default_value(defaultParams.moodCountPerTrack), "Number of moods to assign to each track")
            ("help,h", "produce help message")
            ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, options), vm);

        if (vm.count("help")) {
            std::cout << options << "\n";
            return EXIT_SUCCESS;
        }

        // notify required params
        po::notify(vm);

        GeneratorParameters genParams;
        genParams.releaseCountPerBatch = vm["release-count-per-batch"].as<unsigned>();
        genParams.releaseCount = vm["release-count"].as<unsigned>();
        genParams.trackCountPerRelease = vm["track-count-per-release"].as<unsigned>();
        genParams.compilationRatio = vm["compilation-ratio"].as<float>();
        genParams.trackPath = std::filesystem::path{ vm["track-path"].as<std::string>() };

        if (!std::filesystem::exists(genParams.trackPath))
            throw std::runtime_error{ "File '" + genParams.trackPath.string() + "' does not exist!" };

        Service<IConfig> config{ createConfig(vm["conf"].as<std::string>()) };
        Database::Db db{ config->getPath("working-dir") / "lms.db" };
        Database::Session session{ db };
        std::cout << "Starting generation..." << std::endl;

        GenerationContext genContext{ session };
        prepareContext(genParams, genContext);
        generate(genParams, genContext);

        std::cout << "Generation complete!" << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
