/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "FeaturesEngine.hpp"

#include <memory>
#include <random>

#include "core/ILogger.hpp"

#include "audio/AudioFeatures.hpp"

#include "core/math/StatsAccumulator.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackAudioFeatures.hpp"
#include "features/FeaturesDefs.hpp"
#include "som/Matrix.hpp"
#include "som/Trainer.hpp"

namespace lms::recommendation
{
    namespace
    {
        void flattenFeatures(const audio::AudioFeatures& features, AudioFeatureVector& somInput)
        {
            std::size_t featureIndex{};
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i, ++featureIndex)
                somInput[featureIndex] = features.logMelEnergyMean[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i, ++featureIndex)
                somInput[featureIndex] = features.logMelEnergyStdDev[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i, ++featureIndex)
                somInput[featureIndex] = features.logMelEnergySkewness[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i, ++featureIndex)
                somInput[featureIndex] = features.logMelEnergyDeltaStdDev[i];
        }

        void normalizeFeatures(AudioFeatureVector& somInput, const AudioFeatureVector& featureMeans, const AudioFeatureVector& featureStdDevs)
        {
            for (std::size_t i{}; i < AudioFeatureVector::getSize(); ++i)
            {
                constexpr float epsilon{ 1e-5F };

                if (featureStdDevs[i] > epsilon)
                    somInput[i] = (somInput[i] - featureMeans[i]) / featureStdDevs[i];
                else
                    somInput[i] = 0;
            }
        }
    } // namespace

    std::unique_ptr<IEngine> createFeaturesEngine(db::IDb& db)
    {
        return std::make_unique<FeaturesEngine>(db);
    }

    FeaturesEngine::FeaturesEngine(db::IDb& db)
        : _db{ db }
        , _ioContextRunner{ _ioContext, 1, "FeaturesEngine" }
    {
    }

    FeaturesEngine::~FeaturesEngine()
    {
        abort();
    }

    void FeaturesEngine::requestReload()
    {
        _ioContext.post([this] { train(); });
    }

    TrackContainer FeaturesEngine::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        return {};
    }

    TrackContainer FeaturesEngine::findSimilarTracks(const std::vector<db::TrackId>& tracksId, std::size_t maxCount) const
    {
        return {};
    }

    ReleaseContainer FeaturesEngine::getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        return {};
    }

    ArtistContainer FeaturesEngine::getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        return {};
    }

    void FeaturesEngine::abort()
    {
        _abortRequested = true;
        _ioContextRunner.wait();
    }

    void FeaturesEngine::train()
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        audio::AudioFeatures audioFeatures{}; // cache values
        AudioFeatureVector somInput;          // cache values
        std::size_t trackCount{};

        // Compute stats over the dataset
        {
            std::array<core::math::StatsAccumulator, audioFeatureCount> statsAccumulators;

            db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
                audio::audioFeaturesFromBlob(features->getData(), audioFeatures);
                flattenFeatures(audioFeatures, somInput);
                for (std::size_t i{}; i < audioFeatureCount; ++i)
                    statsAccumulators[i].add(somInput[i]);

                trackCount++;
            });

            for (std::size_t featureIndex{}; featureIndex < audioFeatureCount; ++featureIndex)
            {
                _featureMeans[featureIndex] = static_cast<audio::FeatureValue>(statsAccumulators[featureIndex].getMean());
                _featureStdDevs[featureIndex] = static_cast<audio::FeatureValue>(statsAccumulators[featureIndex].getSampleStdDev());
            }
        }

        LMS_LOG(RECOMMENDATION, DEBUG, "Audio features stats computed on " << trackCount << " tracks");

        som::Coordinate somSize{ std::max<som::Coordinate>(5, std::round(3.F * std::pow(static_cast<float>(trackCount), 0.25F))) };
        LMS_LOG(RECOMMENDATION, DEBUG, "Training a " << somSize << "*" << somSize << " network...");

        // We want the training to be deterministic for now (to better spot effects of parameter changes)
        std::minstd_rand randomEngine{ 42 };
        AudioSom som{ somSize, somSize, randomEngine, -1.F, 1.F };

        constexpr std::size_t epochCount{ 30 };

        som::Trainer trainer{ som, som::TrainerParams{ .epochCount = epochCount } };

        auto allTrackFeatureIds{ db::TrackAudioFeatures::find(session) };
        for (std::size_t i{}; i < epochCount; ++i)
        {
            std::shuffle(std::begin(allTrackFeatureIds.results), std::end(allTrackFeatureIds.results), randomEngine);

            trainer.beginEpoch();
            for (db::TrackAudioFeaturesId trackFeaturesId : allTrackFeatureIds.results)
            {
                const db::TrackAudioFeatures::pointer features{ db::TrackAudioFeatures::find(session, trackFeaturesId) };

                // db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
                audio::audioFeaturesFromBlob(features->getData(), audioFeatures);

                // Flatten features into a single vector and normalize them
                flattenFeatures(audioFeatures, somInput);
                normalizeFeatures(somInput, _featureMeans, _featureStdDevs);
                somInput.normalizeL2();

                trainer.train(somInput);
            }

            LMS_LOG(RECOMMENDATION, DEBUG, "Epoch " << i + 1 << "/" << epochCount << " done");
        }

        som::Matrix<std::vector<db::TrackId>> trackMap{ som.getWidth(), som.getHeight() };

        // Now we should be able to cluster all tracks using the trained SOM
        db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
            audio::audioFeaturesFromBlob(features->getData(), audioFeatures);

            // Flatten features into a single vector and normalize them
            flattenFeatures(audioFeatures, somInput);
            normalizeFeatures(somInput, _featureMeans, _featureStdDevs);
            somInput.normalizeL2();

            const som::MatrixPosition pos{ som.getBestMatchingNeuron(somInput) };
            trackMap.get(pos).push_back(features->getTrackId());
        });

        LMS_LOG(RECOMMENDATION, INFO, "Training complete!");
    }
} // namespace lms::recommendation
