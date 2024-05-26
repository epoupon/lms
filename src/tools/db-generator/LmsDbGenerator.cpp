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
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <stdlib.h>
#include <string>

#include <boost/program_options.hpp>

#include "core/IConfig.hpp"
#include "core/Random.hpp"
#include "core/Service.hpp"
#include "core/StreamLogger.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"

namespace lms
{
    struct GeneratorParameters
    {
        std::size_t mediaLibraryCount{ 1 };
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
        db::Session& session;
        std::vector<db::MediaLibrary::pointer> mediaLibraries;
        std::vector<db::Cluster::pointer> genres;
        std::vector<db::Cluster::pointer> moods;
        GenerationContext(db::Session& _session)
            : session{ _session } {}
    };

    db::Cluster::pointer generateCluster(db::Session& session, db::ClusterType::pointer clusterType)
    {
        const std::string clusterName{ std::string{ clusterType->getName() } + "-" + std::string{ core::UUID::generate().getAsString() } };
        return session.create<db::Cluster>(clusterType, clusterName);
    }

    db::Artist::pointer generateArtist(db::Session& session)
    {
        const core::UUID artistMBID{ core::UUID::generate() };
        const std::string artistName{ "Artist-" + std::string{ core::UUID::generate().getAsString() } };
        return session.create<db::Artist>(artistName, artistMBID);
    }

    void generateRelease(const GeneratorParameters& params, GenerationContext& context)
    {
        using namespace db;

        const core::UUID releaseMBID{ core::UUID::generate() };
        const std::string releaseName{ "Release-" + std::string{ core::UUID::generate().getAsString() } };
        Release::pointer release{ context.session.create<Release>(releaseName, releaseMBID) };

        Artist::pointer artist{ generateArtist(context.session) };

        MediaLibrary::pointer mediaLibrary;
        if (!context.mediaLibraries.empty())
            mediaLibrary = *core::random::pickRandom(context.mediaLibraries);

        for (std::size_t i{}; i < params.trackCountPerRelease; ++i)
        {
            Track::pointer track{ context.session.create<Track>() };

            track.modify()->setName("Track-" + std::string{ core::UUID::generate().getAsString() });
            track.modify()->setDiscNumber(1);
            track.modify()->setTrackNumber(i);
            track.modify()->setDuration(std::chrono::seconds{ core::random::getRandom(30, 300) });
            track.modify()->setRelease(release);
            track.modify()->setTrackMBID(core::UUID::generate());
            track.modify()->setRecordingMBID(core::UUID::generate());
            track.modify()->setTotalTrack(params.trackCountPerRelease);
            if (mediaLibrary)
                track.modify()->setMediaLibrary(mediaLibrary);

            TrackArtistLink::create(context.session, track, artist, TrackArtistLinkType::Artist);
            TrackArtistLink::create(context.session, track, artist, TrackArtistLinkType::ReleaseArtist);

            std::vector<ObjectPtr<Cluster>> clusters;
            if (!context.genres.empty())
                clusters.push_back(*core::random::pickRandom(context.genres));
            if (!context.moods.empty())
                clusters.push_back(*core::random::pickRandom(context.moods));
            track.modify()->setClusters(clusters);
        }
    }

    void generate(const GeneratorParameters& params, GenerationContext& context)
    {
        std::size_t remainingCount{ params.releaseCount };

        while (remainingCount > 0)
        {
            auto transaction{ context.session.createWriteTransaction() };
            std::cout << "Generating album #" << params.releaseCount - remainingCount << " / " << params.releaseCount << std::endl;

            for (std::size_t i{}; i < params.releaseCountPerBatch && remainingCount-- > 0; ++i)
                generateRelease(params, context);
        }
    }

    void prepareContext(const GeneratorParameters& params, GenerationContext& context)
    {
        auto transaction{ context.session.createWriteTransaction() };

        // create some random media libraries
        for (std::size_t i{}; i < params.mediaLibraryCount; ++i)
            context.mediaLibraries.push_back(context.session.create<db::MediaLibrary>());

        // create some random genres/moods
        {
            db::ClusterType::pointer genre{ db::ClusterType::find(context.session, "GENRE") };
            if (!genre)
                genre = context.session.create<db::ClusterType>("GENRE");

            for (std::size_t i{}; i < params.genreCount; ++i)
                context.genres.push_back(generateCluster(context.session, genre));
        }

        {
            db::ClusterType::pointer mood{ db::ClusterType::find(context.session, "MOOD") };
            if (!mood)
                mood = context.session.create<db::ClusterType>("MOOD");

            for (std::size_t i{}; i < params.moodCount; ++i)
                context.moods.push_back(generateCluster(context.session, mood));
        }
    }
} // namespace lms

int main(int argc, char* argv[])
{
    try
    {
        using namespace lms;
        namespace program_options = boost::program_options;

        // log to stdout
        core::Service<core::logging::ILogger> logger{ std::make_unique<core::logging::StreamLogger>(std::cout) };

        const GeneratorParameters defaultParams;

        program_options::options_description options{ "Options" };
        options.add_options()("conf,c", program_options::value<std::string>()->default_value("/etc/lms.conf"), "lms config file")("media-library-count", program_options::value<unsigned>()->default_value(defaultParams.mediaLibraryCount), "Number of media libraries to use")("release-count-per-batch", program_options::value<unsigned>()->default_value(defaultParams.releaseCountPerBatch), "Number of releases to generate before committing transaction")("release-count", program_options::value<unsigned>()->default_value(defaultParams.releaseCount), "Number of releases to generate")("track-count-per-release", program_options::value<unsigned>()->default_value(defaultParams.trackCountPerRelease), "Number of tracks per release")("compilation-ratio", program_options::value<float>()->default_value(defaultParams.compilationRatio), "Compilation ratio (compilation means all tracks have a different artist)")("track-path", program_options::value<std::string>()->required(), "Path of a valid track file, that will be used for all generated tracks")("genre-count", program_options::value<unsigned>()->default_value(defaultParams.genreCount), "Number of genres to generate")("genre-count-per-track", program_options::value<unsigned>()->default_value(defaultParams.genreCountPerTrack), "Number of genres to assign to each track")("mood-count", program_options::value<unsigned>()->default_value(defaultParams.moodCount), "Number of moods to generate")("mood-count-per-track", program_options::value<unsigned>()->default_value(defaultParams.moodCountPerTrack), "Number of moods to assign to each track")("help,h", "produce help message");

        program_options::variables_map vm;
        program_options::store(program_options::parse_command_line(argc, argv, options), vm);

        if (vm.count("help"))
        {
            std::cout << options << "\n";
            return EXIT_SUCCESS;
        }

        // notify required params
        program_options::notify(vm);

        GeneratorParameters genParams;
        genParams.mediaLibraryCount = vm["media-library-count"].as<unsigned>();
        genParams.releaseCountPerBatch = vm["release-count-per-batch"].as<unsigned>();
        genParams.releaseCount = vm["release-count"].as<unsigned>();
        genParams.trackCountPerRelease = vm["track-count-per-release"].as<unsigned>();
        genParams.compilationRatio = vm["compilation-ratio"].as<float>();
        genParams.trackPath = std::filesystem::path{ vm["track-path"].as<std::string>() };

        if (!std::filesystem::exists(genParams.trackPath))
            throw std::runtime_error{ "File '" + genParams.trackPath.string() + "' does not exist!" };

        core::Service<core::IConfig> config{ core::createConfig(vm["conf"].as<std::string>()) };
        db::Db db{ config->getPath("working-dir") / "lms.db" };
        db::Session session{ db };
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
