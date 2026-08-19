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

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <stdlib.h>
#include <string>

#include <boost/program_options.hpp>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Random.hpp"
#include "core/Service.hpp"
#include "core/SystemPaths.hpp"

#include "audio/MusicNNEmbeddings.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"

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
        std::size_t groupingCountPerTrack{ 1 };
        std::size_t languageCountPerTrack{ 1 };
        std::size_t moodCountPerTrack{ 3 };
        std::size_t trackEmbeddedImagePerRelease{ 1 }; // usual case: one same image saved on each track
        std::size_t genreCount{ 50 };
        std::size_t groupingCount{ 10 };
        std::size_t languageCount{ 10 };
        std::size_t moodCount{ 25 };
        bool generateMusicNNEmbeddings{ false };
        std::filesystem::path trackPath;
    };

    struct GenerationContext
    {
        db::Session& session;
        std::vector<db::MediaLibrary::pointer> mediaLibraries;
        std::vector<db::Genre::pointer> genres;
        std::vector<db::Grouping::pointer> groupings;
        std::vector<db::Language::pointer> languages;
        std::vector<db::Mood::pointer> moods;
        GenerationContext(db::Session& _session)
            : session{ _session } {}
    };

    db::Artist::pointer generateArtist(db::Session& session)
    {
        const core::UUID artistMBID{ core::UUID::generate() };
        const std::string artistName{ "Artist-" + core::UUID::generate().toString() };
        return session.create<db::Artist>(artistName, artistMBID);
    }

    void generateRelease(const GeneratorParameters& params, GenerationContext& context)
    {
        using namespace db;

        const core::UUID releaseMBID{ core::UUID::generate() };
        const std::string releaseName{ "Release-" + core::UUID::generate().toString() };
        Release::pointer release{ context.session.create<Release>(releaseName, releaseMBID) };
        Medium::pointer medium{ context.session.create<Medium>(release) };
        medium.modify()->setTrackCount(params.trackCountPerRelease);

        Artist::pointer artist{ generateArtist(context.session) };

        MediaLibrary::pointer mediaLibrary;
        if (!context.mediaLibraries.empty())
            mediaLibrary = *core::random::pickRandom(context.mediaLibraries);

        std::vector<TrackEmbeddedImage::pointer> trackEmbeddedImages;
        for (std::size_t i{}; i < params.trackEmbeddedImagePerRelease; ++i)
            trackEmbeddedImages.push_back(context.session.create<TrackEmbeddedImage>());

        for (std::size_t i{}; i < params.trackCountPerRelease; ++i)
        {
            Track::pointer track{ context.session.create<Track>() };

            track.modify()->setName("Track-" + core::UUID::generate().toString());
            track.modify()->setMedium(medium);
            track.modify()->setTrackNumber(i);
            track.modify()->setDuration(std::chrono::seconds{ core::random::getRandom(30, 300) });
            track.modify()->setRelease(release);
            track.modify()->setTrackMBID(core::UUID::generate());
            track.modify()->setRecordingMBID(core::UUID::generate());
            if (mediaLibrary)
                track.modify()->setMediaLibrary(mediaLibrary);

            context.session.create<TrackArtistLink>(track, artist, TrackArtistLinkType::Artist);
            context.session.create<ReleaseArtistLink>(release, artist, false);

            if (!trackEmbeddedImages.empty())
                context.session.create<TrackEmbeddedImageLink>(track, *core::random::pickRandom(trackEmbeddedImages));

            auto pickN = [](const auto& pool, std::size_t n) {
                using T = typename std::decay_t<decltype(pool)>::value_type;
                std::vector<std::size_t> indices(pool.size());
                std::iota(indices.begin(), indices.end(), std::size_t{});
                std::shuffle(indices.begin(), indices.end(), core::random::getRandGenerator());
                const std::size_t count{ std::min(n, pool.size()) };
                std::vector<T> picked;
                picked.reserve(count);
                for (std::size_t k{}; k < count; ++k)
                    picked.push_back(pool[indices[k]]);
                return picked;
            };

            if (!context.genres.empty())
                track.modify()->setGenres(pickN(context.genres, params.genreCountPerTrack));
            if (!context.groupings.empty())
                track.modify()->setGroupings(pickN(context.groupings, params.groupingCountPerTrack));
            if (!context.languages.empty())
                track.modify()->setLanguages(pickN(context.languages, params.languageCountPerTrack));
            if (!context.moods.empty())
                track.modify()->setMoods(pickN(context.moods, params.moodCountPerTrack));

            if (params.generateMusicNNEmbeddings)
            {
                audio::TrackMusicNNEmbeddings embeddings;
                std::normal_distribution<float> embeddingDist{ 0.0F, 1.0F };
                for (auto& v : embeddings.mean.values)
                    v = embeddingDist(core::random::getRandGenerator());
                std::vector<std::byte> blob(sizeof(audio::TrackMusicNNEmbeddings));
                audio::trackMusicNNEmbeddingsToBlob(embeddings, blob);
                TrackMusicNNEmbeddings::pointer entry{ context.session.create<TrackMusicNNEmbeddings>(track) };
                entry.modify()->setData(blob);
            }
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
            context.mediaLibraries.push_back(context.session.create<db::MediaLibrary>("Library" + std::to_string(i), "/root" + std::to_string(i)));

        // create some random genres/groupings/languages/moods
        for (std::size_t i{}; i < params.genreCount; ++i)
            context.genres.push_back(context.session.create<db::Genre>("Genre-" + core::UUID::generate().toString()));

        for (std::size_t i{}; i < params.groupingCount; ++i)
            context.groupings.push_back(context.session.create<db::Grouping>("Grouping-" + core::UUID::generate().toString()));

        for (std::size_t i{}; i < params.languageCount; ++i)
            context.languages.push_back(context.session.create<db::Language>("Language-" + core::UUID::generate().toString()));

        for (std::size_t i{}; i < params.moodCount; ++i)
            context.moods.push_back(context.session.create<db::Mood>("Mood-" + core::UUID::generate().toString()));
    }
} // namespace lms

int main(int argc, char* argv[])
{
    try
    {
        using namespace lms;
        namespace program_options = boost::program_options;

        // log to stdout
        core::Service<core::logging::ILogger> logger{ core::logging::createLogger() };

        const GeneratorParameters defaultParams;

        program_options::options_description options{ "Options" };

        // clang-format off
        options.add_options()
        ("conf,c", program_options::value<std::string>()->default_value(core::sysconfDirectory / "lms.conf"), "lms config file")
        ("media-library-count", program_options::value<unsigned>()->default_value(defaultParams.mediaLibraryCount), "Number of media libraries to use")
        ("release-count-per-batch",program_options::value<unsigned>()->default_value(defaultParams.releaseCountPerBatch), "Number of releases to generate before committing transaction")
        ("release-count",program_options::value<unsigned>()->default_value(defaultParams.releaseCount), "Number of releases to generate")
        ("track-count-per-release", program_options::value<unsigned>()->default_value(defaultParams.trackCountPerRelease), "Number of tracks per release")
        ("track-embedded-image-count",program_options::value<unsigned>()->default_value(defaultParams.trackEmbeddedImagePerRelease), "Number of different embedded track images for the whole release (each track has one different embedded image)")
        ("compilation-ratio",program_options::value<float>()->default_value(defaultParams.compilationRatio), "Compilation ratio (compilation means all tracks have a different artist)")
        ("track-path",program_options::value<std::string>()->required(), "Path of a valid track file, that will be used for all generated tracks")
        ("genre-count", program_options::value<unsigned>()->default_value(defaultParams.genreCount), "Number of genres to generate")
        ("genre-count-per-track", program_options::value<unsigned>()->default_value(defaultParams.genreCountPerTrack), "Number of genres to assign to each track")
        ("grouping-count", program_options::value<unsigned>()->default_value(defaultParams.groupingCount), "Number of groupings to generate")
        ("grouping-count-per-track", program_options::value<unsigned>()->default_value(defaultParams.groupingCountPerTrack), "Number of groupings to assign to each track")
        ("language-count", program_options::value<unsigned>()->default_value(defaultParams.languageCount), "Number of languages to generate")
        ("language-count-per-track", program_options::value<unsigned>()->default_value(defaultParams.languageCountPerTrack), "Number of languages to assign to each track")
        ("mood-count", program_options::value<unsigned>()->default_value(defaultParams.moodCount), "Number of moods to generate")
        ("mood-count-per-track", program_options::value<unsigned>()->default_value(defaultParams.moodCountPerTrack), "Number of moods to assign to each track")
        ("musicnn-embeddings", program_options::bool_switch()->default_value(false), "Generate fake MusicNN embeddings for each track")
        ("help,h", "produce help message");
        // clang-format on

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
        genParams.trackEmbeddedImagePerRelease = vm["track-embedded-image-count"].as<unsigned>();
        genParams.generateMusicNNEmbeddings = vm["musicnn-embeddings"].as<bool>();
        genParams.trackPath = std::filesystem::path{ vm["track-path"].as<std::string>() };
        genParams.genreCount = vm["genre-count"].as<unsigned>();
        genParams.genreCountPerTrack = vm["genre-count-per-track"].as<unsigned>();
        genParams.groupingCount = vm["grouping-count"].as<unsigned>();
        genParams.groupingCountPerTrack = vm["grouping-count-per-track"].as<unsigned>();
        genParams.languageCount = vm["language-count"].as<unsigned>();
        genParams.languageCountPerTrack = vm["language-count-per-track"].as<unsigned>();
        genParams.moodCount = vm["mood-count"].as<unsigned>();
        genParams.moodCountPerTrack = vm["mood-count-per-track"].as<unsigned>();

        if (!std::filesystem::exists(genParams.trackPath))
            throw std::runtime_error{ "File '" + genParams.trackPath.string() + "' does not exist!" };

        core::Service<core::IConfig> config{ core::createConfig(vm["conf"].as<std::string>()) };
        auto db{ db::createDb(config->getPath("working-dir", "/var/lms") / "lms.db") };
        db::Session session{ *db };
        session.prepareTablesIfNeeded();
        session.migrateSchemaIfNeeded();
        session.createScanSettingsIfNeeded();
        session.createIndexesIfNeeded();

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
