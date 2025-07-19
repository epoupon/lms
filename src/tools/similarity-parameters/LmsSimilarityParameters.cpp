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
#include <string>

#include "core/Config.hpp"
#include "core/Service.hpp"
#include "core/StreamLogger.hpp"
#include "database/IDb.hpp"
#include "database/SessionPool.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "similarity/features/SimilarityFeaturesSearcher.hpp"

#include "GeneticAlgorithm.hpp"

using namespace Similarity;
using SimilarityScore = GeneticAlgorithm<FeatureSettingsMap>::Score;

// An individual is just a FeatureSettingsMap
// The goal is to get the FeatureSettingsMap that maximize the score
const FeatureSettingsMap featuresSettings{
    { "lowlevel.average_loudness", { 1 } },
    { "lowlevel.barkbands.mean", { 1 } },
    { "lowlevel.barkbands.median", { 1 } },
    { "lowlevel.barkbands.var", { 1 } },
    { "lowlevel.barkbands_crest.mean", { 1 } },
    { "lowlevel.barkbands_crest.median", { 1 } },
    { "lowlevel.barkbands_crest.var", { 1 } },
    { "lowlevel.barkbands_flatness_db.mean", { 1 } },
    { "lowlevel.barkbands_flatness_db.median", { 1 } },
    { "lowlevel.barkbands_flatness_db.var", { 1 } },
    { "lowlevel.barkbands_kurtosis.mean", { 1 } },
    { "lowlevel.barkbands_kurtosis.median", { 1 } },
    { "lowlevel.barkbands_kurtosis.var", { 1 } },
    { "lowlevel.barkbands_skewness.mean", { 1 } },
    { "lowlevel.barkbands_skewness.median", { 1 } },
    { "lowlevel.barkbands_skewness.var", { 1 } },
    { "lowlevel.barkbands_spread.mean", { 1 } },
    { "lowlevel.barkbands_spread.median", { 1 } },
    { "lowlevel.barkbands_spread.var", { 1 } },
    { "lowlevel.dissonance.mean", { 1 } },
    { "lowlevel.dissonance.median", { 1 } },
    { "lowlevel.dissonance.var", { 1 } },
    { "lowlevel.dynamic_complexity", { 1 } },
    { "lowlevel.spectral_contrast_coeffs.mean", { 1 } },
    { "lowlevel.spectral_contrast_coeffs.median", { 1 } },
    { "lowlevel.spectral_contrast_coeffs.var", { 1 } },
    { "lowlevel.erbbands.mean", { 1 } },
    { "lowlevel.erbbands.median", { 1 } },
    { "lowlevel.erbbands.var", { 1 } },
    { "lowlevel.gfcc.mean", { 1 } },
    { "lowlevel.hfc.mean", { 1 } },
    { "lowlevel.hfc.median", { 1 } },
    { "lowlevel.hfc.var", { 1 } },
    { "tonal.hpcp.median", { 1 } },
    { "lowlevel.melbands.mean", { 1 } },
    { "lowlevel.melbands.median", { 1 } },
    { "lowlevel.melbands.var", { 1 } },
    { "lowlevel.melbands_crest.mean", { 1 } },
    { "lowlevel.melbands_crest.median", { 1 } },
    { "lowlevel.melbands_crest.var", { 1 } },
    { "lowlevel.melbands_flatness_db.mean", { 1 } },
    { "lowlevel.melbands_flatness_db.median", { 1 } },
    { "lowlevel.melbands_flatness_db.var", { 1 } },
    { "lowlevel.melbands_kurtosis.mean", { 1 } },
    { "lowlevel.melbands_kurtosis.median", { 1 } },
    { "lowlevel.melbands_kurtosis.var", { 1 } },
    { "lowlevel.melbands_skewness.mean", { 1 } },
    { "lowlevel.melbands_skewness.median", { 1 } },
    { "lowlevel.melbands_skewness.var", { 1 } },
    { "lowlevel.melbands_spread.mean", { 1 } },
    { "lowlevel.melbands_spread.median", { 1 } },
    { "lowlevel.melbands_spread.var", { 1 } },
    { "lowlevel.mfcc.mean", { 1 } },
    { "lowlevel.pitch_salience.mean", { 1 } },
    { "lowlevel.pitch_salience.median", { 1 } },
    { "lowlevel.pitch_salience.var", { 1 } },
    { "lowlevel.silence_rate_30dB.mean", { 1 } },
    { "lowlevel.silence_rate_30dB.median", { 1 } },
    { "lowlevel.silence_rate_30dB.var", { 1 } },
    { "lowlevel.silence_rate_60dB.mean", { 1 } },
    { "lowlevel.silence_rate_60dB.median", { 1 } },
    { "lowlevel.silence_rate_60dB.var", { 1 } },
    { "lowlevel.spectral_centroid.mean", { 1 } },
    { "lowlevel.spectral_centroid.median", { 1 } },
    { "lowlevel.spectral_centroid.var", { 1 } },
    { "lowlevel.spectral_complexity.mean", { 1 } },
    { "lowlevel.spectral_complexity.median", { 1 } },
    { "lowlevel.spectral_complexity.var", { 1 } },
    { "lowlevel.spectral_contrast_coeffs.mean", { 1 } },
    { "lowlevel.spectral_contrast_coeffs.median", { 1 } },
    { "lowlevel.spectral_contrast_coeffs.var", { 1 } },
    { "lowlevel.spectral_contrast_valleys.mean", { 1 } },
    { "lowlevel.spectral_contrast_valleys.median", { 1 } },
    { "lowlevel.spectral_contrast_valleys.var", { 1 } },
    { "lowlevel.spectral_decrease.mean", { 1 } },
    { "lowlevel.spectral_decrease.median", { 1 } },
    { "lowlevel.spectral_decrease.var", { 1 } },
    { "lowlevel.spectral_energy.mean", { 1 } },
    { "lowlevel.spectral_energy.median", { 1 } },
    { "lowlevel.spectral_energy.var", { 1 } },
    { "lowlevel.spectral_energyband_high.mean", { 1 } },
    { "lowlevel.spectral_energyband_high.median", { 1 } },
    { "lowlevel.spectral_energyband_high.var", { 1 } },
    { "lowlevel.spectral_energyband_low.mean", { 1 } },
    { "lowlevel.spectral_energyband_low.median", { 1 } },
    { "lowlevel.spectral_energyband_low.var", { 1 } },
    { "lowlevel.spectral_energyband_middle_high.mean", { 1 } },
    { "lowlevel.spectral_energyband_middle_high.median", { 1 } },
    { "lowlevel.spectral_energyband_middle_high.var", { 1 } },
    { "lowlevel.spectral_energyband_middle_low.mean", { 1 } },
    { "lowlevel.spectral_energyband_middle_low.median", { 1 } },
    { "lowlevel.spectral_energyband_middle_low.var", { 1 } },
    { "lowlevel.spectral_entropy.mean", { 1 } },
    { "lowlevel.spectral_entropy.median", { 1 } },
    { "lowlevel.spectral_entropy.var", { 1 } },
    { "lowlevel.spectral_flux.mean", { 1 } },
    { "lowlevel.spectral_flux.median", { 1 } },
    { "lowlevel.spectral_flux.var", { 1 } },
    { "lowlevel.spectral_kurtosis.mean", { 1 } },
    { "lowlevel.spectral_kurtosis.median", { 1 } },
    { "lowlevel.spectral_kurtosis.var", { 1 } },
    { "lowlevel.spectral_rms.mean", { 1 } },
    { "lowlevel.spectral_rms.median", { 1 } },
    { "lowlevel.spectral_rms.var", { 1 } },
    { "lowlevel.spectral_rolloff.mean", { 1 } },
    { "lowlevel.spectral_rolloff.median", { 1 } },
    { "lowlevel.spectral_rolloff.var", { 1 } },
    { "lowlevel.spectral_skewness.mean", { 1 } },
    { "lowlevel.spectral_skewness.median", { 1 } },
    { "lowlevel.spectral_skewness.var", { 1 } },
    { "lowlevel.spectral_spread.mean", { 1 } },
    { "lowlevel.spectral_spread.median", { 1 } },
    { "lowlevel.spectral_spread.var", { 1 } },
    { "lowlevel.zerocrossingrate.mean", { 1 } },
    { "lowlevel.zerocrossingrate.median", { 1 } },
    { "lowlevel.zerocrossingrate.var", { 1 } },
};

static std::unordered_map<db::IdType, FeatureValuesMap>
constructFeaturesCache(db::Session& session, const FeatureSettingsMap& featureSettings)
{
    std::unordered_map<db::IdType, FeatureValuesMap> cache;

    std::unordered_set<FeatureName> names;
    std::transform(std::cbegin(featureSettings), std::cend(featureSettings), std::inserter(names, std::begin(names)),
        [](const auto& itFeature) { return itFeature.first; });

    auto transaction{ session.createReadTransaction() };

    for (auto trackId : db::Track::getAllIdsWithFeatures(session))
    {
        const db::Track::pointer track{ db::Track::getById(session, trackId) };
        const db::TrackFeatures::pointer trackFeatures{ track->getTrackFeatures() };

        cache[trackId] = trackFeatures->getFeatureValuesMap(names);
    }

    return cache;
}

static std::optional<FeatureValuesMap>
getFeaturesFromCache(const std::unordered_map<db::IdType, FeatureValuesMap>& cache, db::IdType trackId, const FeatureNames& names)
{
    std::optional<FeatureValuesMap> res;

    auto it{ cache.find(trackId) };
    if (it == std::cend(cache))
        return res;

    res = FeatureValuesMap{};

    const FeatureValuesMap& trackFeatures{ it->second };
    for (const FeatureName& name : names)
    {
        auto itFeatures{ trackFeatures.find(name) };
        if (itFeatures == std::cend(trackFeatures))
        {
            res.reset();
            break;
        }

        res->emplace(name, itFeatures->second);
    }

    return res;
}

static void
printFeatureSettingsMap(const FeatureSettingsMap& featureSettings)
{
    std::cout << "FeatureSettingsMap: (" << featureSettings.size() << " features)" << std::endl;
    for (const auto& [name, settings] : featureSettings)
        std::cout << "\t" << name << std::endl;
}

static std::string
trackToString(db::Session& session, db::IdType trackId)
{
    std::string res;
    auto transaction{ session.createReadTransaction() };
    db::Track::pointer track{ db::Track::getById(session, trackId) };

    res += track->getName();
    if (track->getRelease())
        res += " [" + track->getRelease()->getName() + "]";
    for (auto artist : track->getArtists())
        res += " - " + artist->getName();
    for (auto cluster : track->getClusters())
        res += " {" + cluster->getType()->getName() + "-" + cluster->getName() + "}";

    return res;
}

static SimilarityScore
computeTrackScore(db::Session& session, db::IdType track1Id, db::IdType track2Id)
{
    SimilarityScore score{};

    auto transaction{ session.createReadTransaction() };

    auto track1{ db::Track::getById(session, track1Id) };
    auto track2{ db::Track::getById(session, track2Id) };

    if (track1->getRelease() == track2->getRelease())
        score += 1;

    // Artists in common
    {
        auto track1ArtistIds{ track1->getArtistIds() };
        auto track2ArtistIds{ track2->getArtistIds() };

        std::vector<db::IdType> commonArtistIds;
        std::set_intersection(std::cbegin(track1ArtistIds), std::cend(track1ArtistIds),
            std::cbegin(track2ArtistIds), std::cend(track2ArtistIds),
            std::back_inserter(commonArtistIds));

        score += commonArtistIds.size();
    }

    // Clusters in common
    {
        auto track1ClusterIds{ track1->getClusterIds() };
        auto track2ClusterIds{ track2->getClusterIds() };

        std::vector<db::IdType> commonClusterIds;
        std::set_intersection(std::cbegin(track1ClusterIds), std::cend(track1ClusterIds),
            std::cbegin(track2ClusterIds), std::cend(track2ClusterIds),
            std::back_inserter(commonClusterIds));

        score += commonClusterIds.size();
    }

    return score;
}

static SimilarityScore
computeSimilarityScore(db::Session& session, FeaturesSearcher::TrainSettings trainSettings)
{
    std::cout << "Compute score of: ";
    printFeatureSettingsMap(trainSettings.featureSettingsMap);
    std::cout << std::endl;

    FeaturesSearcher searcher{ session, trainSettings };

    const std::vector<db::IdType> trackIds = std::invoke([&]() {
        auto transaction{ session.createReadTransaction() };
        return db::Track::getAllIdsWithFeatures(session);
    });

    SimilarityScore score{};
    for (db::IdType trackId : trackIds)
    {
        constexpr std::size_t nbSimilarTracks{ 3 };
        //		std::cout << "Processing track '" << trackToString(session, trackId) << "'" << std::endl;
        SimilarityScore factor{ 1 };
        for (db::IdType similarTrackId : searcher.getSimilarTracks({ trackId }, nbSimilarTracks))
        {
            SimilarityScore trackScore{ computeTrackScore(session, trackId, similarTrackId) };
            //			std::cout << "\tScore = " << trackScore << " (*" << factor << ") with track '" << trackToString(session, similarTrackId) << "'" << std::endl;
            trackScore *= factor;
            score += trackScore;

            factor -= (SimilarityScore{ 1 } / nbSimilarTracks);
        }
    }

    std::cout << "Total score = " << score << std::endl;

    return score;
}

static void
printBadlyClassifiedTracks(db::Session& session, FeaturesSearcher::TrainSettings trainSettings)
{
    FeaturesSearcher searcher{ session, trainSettings };

    const std::vector<db::IdType> trackIds = std::invoke([&]() {
        auto transaction{ session.createReadTransaction() };
        return db::Track::getAllIdsWithFeatures(session);
    });

    for (db::IdType trackId : trackIds)
    {
        constexpr std::size_t nbSimilarTracks{ 3 };
        for (db::IdType similarTrackId : searcher.getSimilarTracks({ trackId }, nbSimilarTracks))
        {
            SimilarityScore trackScore{ computeTrackScore(session, trackId, similarTrackId) };
            if (trackScore == 0)
                std::cout << "Badly classified tracks: '" << trackToString(session, trackId) << "'\n\twith track '" << trackToString(session, similarTrackId) << "'" << std::endl;
        }
    }
}

static FeatureSettingsMap
breedFeatureSettingsMap(const FeatureSettingsMap& a, const FeatureSettingsMap& b)
{
    FeatureSettingsMap res;

    res.insert(std::cbegin(a), std::cend(a));
    res.insert(std::cbegin(b), std::cend(b));

    // just kill random elements until size is good
    while (res.size() > a.size())
    {
        const auto itFeature{ core::random::pickRandom(res) };
        res.erase(itFeature);
    }

    return res;
}

static void
mutateFeatureSettingsMap(FeatureSettingsMap& a)
{
    const std::size_t size{ a.size() };
    // Replace one of the feature with another one, random
    a.erase(core::random::pickRandom(a));

    while (a.size() != size)
    {
        const auto itFeatureSetting{ core::random::pickRandom(featuresSettings) };
        a.emplace(itFeatureSetting->first, itFeatureSetting->second);
    }
}

int main(int argc, char* argv[])
{
    try
    {
        // log to stdout
        //		ServiceProvider<Logger>::create<StreamLogger>(std::cout);

        if (argc != 3)
        {
            std::cerr << "usage: <lms_conf_file> <nb_workers>" << std::endl;
            return EXIT_FAILURE;
        }

        const std::filesystem::path configFilePath{ std::string(argv[1], 0, 256) };
        const std::size_t nbWorkers = atoi(argv[2]);

        ServiceProvider<Config>::create(configFilePath);

        db::Db db{ ServiceProvider<Config>::get()->getPath("working-dir", "/var/lms") / "lms.db" };
        db::SessionPool sessionPool{ db, nbWorkers };

        std::cout << "Caching all features..." << std::endl;
        // Cache all the features of all the music in order to speed up the multiple trainings
        const auto cachedFeatures{ constructFeaturesCache(db::SessionPool::ScopedSession{ sessionPool }.get(), featuresSettings) };
        std::cout << "Caching all features DONE" << std::endl;

        FeaturesSearcher::setFeaturesFetchFunc(
            [&](db::IdType trackId, const FeatureNames& featureNames) {
                return getFeaturesFromCache(cachedFeatures, trackId, featureNames);
            });

        // Create some random settings (i.e random population)
        std::vector<FeatureSettingsMap> initialPopulation;

        constexpr std::size_t populationSize{ 200 };
        constexpr std::size_t nbFeatures{ 5 };

        for (std::size_t i{}; i < populationSize; ++i)
        {
            FeatureSettingsMap settings;

            while (settings.size() < nbFeatures)
            {
                const auto itFeatureSetting{ core::random::pickRandom(featuresSettings) };
                settings.emplace(itFeatureSetting->first, itFeatureSetting->second);
            }

            initialPopulation.emplace_back(std::move(settings));
        }

        FeaturesSearcher::TrainSettings trainSettings;
        trainSettings.iterationCount = 8;
        trainSettings.sampleCountPerNeuron = 1.5;

        GeneticAlgorithm<FeatureSettingsMap>::Params params;
        params.nbWorkers = nbWorkers;
        params.nbGenerations = 1;
        params.crossoverRatio = 0.78;
        params.mutationProbability = 0.2;
        params.breedFunction = breedFeatureSettingsMap;
        params.mutateFunction = mutateFeatureSettingsMap;
        params.scoreFunction =
            [&](const FeatureSettingsMap& featureSettings) {
                FeaturesSearcher::TrainSettings settings{ trainSettings };
                settings.featureSettingsMap = featureSettings;

                db::SessionPool::ScopedSession scopedSession{ sessionPool };
                return computeSimilarityScore(scopedSession.get(), settings);
            };

        GeneticAlgorithm<FeatureSettingsMap> geneticAlgorithm{ params };

        std::cout << "Parameters:\n"
                  << "\tnb total settings = " << featuresSettings.size() << "\n"
                  << "\tnb generations = " << params.nbGenerations << "\n"
                  << "\tpopulationSize = " << populationSize << "\n"
                  << "\tnbFeatures = " << nbFeatures << "\n"
                  << "\tcrossoverRatio = " << params.crossoverRatio << "\n"
                  << "\tmutationProbability = " << params.mutationProbability << "\n"
                  << std::endl;

        std::cout << "Starting simulation..." << std::endl;
        const FeatureSettingsMap selectedSettings{ geneticAlgorithm.simulate(initialPopulation) };
        std::cout << "Simulation complete! Best result:" << std::endl;
        printFeatureSettingsMap(selectedSettings);

        // print all badly classified tracks
        {
            FeaturesSearcher::TrainSettings settings{ trainSettings };
            settings.featureSettingsMap = selectedSettings;

            db::SessionPool::ScopedSession scopedSession{ sessionPool };
            printBadlyClassifiedTracks(scopedSession.get(), settings);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Caught exception: " << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}
