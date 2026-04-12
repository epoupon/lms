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
#include "core/ITraceLogger.hpp"

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
        void flattenAudioFeatures(const audio::AudioFeatures& features, AudioFeatureVector& somInput)
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

        void normalizeAudioFeatures(AudioFeatureVector& somInput, const AudioFeatureVector& featureMeans, const AudioFeatureVector& featureStdDevs)
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
        LMS_SCOPED_TRACE_OVERVIEW("FeaturesEngine", "Training");

        AudioFeatureVector inputVector; // cache values

        computeDatasetStats();
        trainSom();

        som::Matrix<std::vector<db::TrackId>> trackMap{ _som.getWidth(), _som.getHeight() };

        // TODO put this in a dedicated function
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
            getNormalizedAudioFeatureVector(features, inputVector);
            const som::MatrixPosition pos{ _som.getBestMatchingNeuron(inputVector) };
            trackMap.get(pos).push_back(features->getTrackId());
        });

        LMS_LOG(RECOMMENDATION, INFO, "Training complete!");
    }

    void FeaturesEngine::computeDatasetStats()
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Compute dataset stats");

        _trackCount = 0;
        AudioFeatureVector inputVector;
        std::array<core::math::StatsAccumulator, audioFeatureCount> statsAccumulators;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
                getAudioFeatureVector(features, inputVector);
                for (std::size_t i{}; i < audioFeatureCount; ++i)
                    statsAccumulators[i].add(inputVector[i]);

                _trackCount++;
            });
        }

        for (std::size_t featureIndex{}; featureIndex < audioFeatureCount; ++featureIndex)
        {
            _featureMeans[featureIndex] = static_cast<audio::FeatureValue>(statsAccumulators[featureIndex].getMean());
            _featureStdDevs[featureIndex] = static_cast<audio::FeatureValue>(statsAccumulators[featureIndex].getSampleStdDev());
        }

        LMS_LOG(RECOMMENDATION, DEBUG, "Audio features stats computed on " << _trackCount << " tracks");
    }

    void FeaturesEngine::getAudioFeatureVector(const db::ObjectPtr<db::TrackAudioFeatures>& features, AudioFeatureVector& inputVector)
    {
        audio::AudioFeatures audioFeatures;
        audio::audioFeaturesFromBlob(features->getData(), audioFeatures);
        flattenAudioFeatures(audioFeatures, inputVector);
    }

    void FeaturesEngine::getNormalizedAudioFeatureVector(const db::ObjectPtr<db::TrackAudioFeatures>& features, AudioFeatureVector& inputVector) const
    {
        getAudioFeatureVector(features, inputVector);
        normalizeAudioFeatures(inputVector, _featureMeans, _featureStdDevs);
        inputVector.normalizeL2();
    }

    void FeaturesEngine::trainSom()
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Train som");

        constexpr std::size_t epochCount{ 30 };
        std::minstd_rand randomEngine{ 42 }; // We want the training to be deterministic for now (to better spot effects of parameter changes)
        const som::Coordinate somSize{ std::max<som::Coordinate>(5, std::round(3.F * std::pow(static_cast<float>(_trackCount), 0.25F))) };

        LMS_LOG(RECOMMENDATION, DEBUG, "Training a " << somSize << "*" << somSize << " network...");

        _som.resize(somSize, somSize);
        _som.randomize(randomEngine, -1.F, 1.F);

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        auto allTrackFeatureIds{ std::move(db::TrackAudioFeatures::find(session).results) };
        AudioFeatureVector inputVector;

        som::Trainer trainer{ _som, som::TrainerParams{ .epochCount = epochCount } };
        for (std::size_t i{}; i < epochCount; ++i)
        {
            LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Train som epoch");

            std::shuffle(std::begin(allTrackFeatureIds), std::end(allTrackFeatureIds), randomEngine);

            trainer.beginEpoch();
            for (db::TrackAudioFeaturesId trackFeaturesId : allTrackFeatureIds)
            {
                const db::TrackAudioFeatures::pointer features{ db::TrackAudioFeatures::find(session, trackFeaturesId) };
                getNormalizedAudioFeatureVector(features, inputVector);
                trainer.train(inputVector);
            }

            LMS_LOG(RECOMMENDATION, DEBUG, "Epoch " << i + 1 << "/" << epochCount << " done");
        }
    }
} // namespace lms::recommendation
