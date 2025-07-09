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

#include <numeric>

#include "core/ILogger.hpp"
#include "core/Random.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "database/objects/TrackList.hpp"
#include "som/DataNormalizer.hpp"

namespace lms::recommendation
{
    using namespace db;

    std::unique_ptr<IEngine> createFeaturesEngine(db::IDb& db)
    {
        return std::make_unique<FeaturesEngine>(db);
    }

    namespace
    {
        std::optional<som::InputVector> convertFeatureValuesMapToInputVector(const FeatureValuesMap& featureValuesMap, std::size_t nbDimensions)
        {
            std::size_t i{};
            std::optional<som::InputVector> res{ som::InputVector{ nbDimensions } };
            for (const auto& [featureName, values] : featureValuesMap)
            {
                if (values.size() != getFeatureDef(featureName).nbDimensions)
                {
                    LMS_LOG(RECOMMENDATION, WARNING, "Dimension mismatch for feature '" << featureName << "'. Expected " << getFeatureDef(featureName).nbDimensions << ", got " << values.size());
                    res.reset();
                    break;
                }

                for (double val : values)
                    (*res)[i++] = val;
            }

            return res;
        }

        som::InputVector getInputVectorWeights(const FeatureSettingsMap& featureSettingsMap, std::size_t nbDimensions)
        {
            som::InputVector weights{ nbDimensions };
            std::size_t index{};
            for (const auto& [featureName, featureSettings] : featureSettingsMap)
            {
                const std::size_t featureNbDimensions{ getFeatureDef(featureName).nbDimensions };

                for (std::size_t i{}; i < featureNbDimensions; ++i)
                    weights[index++] = (1. / featureNbDimensions * featureSettings.weight);
            }

            assert(index == nbDimensions);

            return weights;
        }
    } // namespace

    const FeatureSettingsMap& FeaturesEngine::getDefaultTrainFeatureSettings()
    {
        static const FeatureSettingsMap defaultTrainFeatureSettings{
            { "lowlevel.spectral_energyband_high.mean", { 1 } },
            { "lowlevel.spectral_rolloff.median", { 1 } },
            { "lowlevel.spectral_contrast_valleys.var", { 1 } },
            { "lowlevel.erbbands.mean", { 1 } },
            { "lowlevel.gfcc.mean", { 1 } },
        };

        return defaultTrainFeatureSettings;
    }

    void FeaturesEngine::loadFromTraining(const TrainSettings& trainSettings, const ProgressCallback& progressCallback)
    {
        LMS_LOG(RECOMMENDATION, INFO, "Constructing features classifier...");

        std::unordered_set<FeatureName> featureNames;
        std::transform(std::cbegin(trainSettings.featureSettingsMap), std::cend(trainSettings.featureSettingsMap), std::inserter(featureNames, std::begin(featureNames)),
            [](const auto& itFeatureSetting) { return itFeatureSetting.first; });

        const std::size_t nbDimensions{ std::accumulate(std::cbegin(featureNames), std::cend(featureNames), std::size_t{ 0 },
            [](std::size_t sum, const FeatureName& featureName) { return sum + getFeatureDef(featureName).nbDimensions; }) };

        LMS_LOG(RECOMMENDATION, DEBUG, "Features dimension = " << nbDimensions);

        Session& session{ _db.getTLSSession() };

        RangeResults<TrackFeaturesId> trackFeaturesIds;
        {
            auto transaction{ session.createReadTransaction() };

            LMS_LOG(RECOMMENDATION, DEBUG, "Getting Track features...");
            trackFeaturesIds = TrackFeatures::find(session);
            LMS_LOG(RECOMMENDATION, DEBUG, "Getting Track features DONE (found " << trackFeaturesIds.results.size() << " track features)");
        }

        std::vector<som::InputVector> samples;
        std::vector<TrackId> samplesTrackIds;

        samples.reserve(trackFeaturesIds.results.size());
        samplesTrackIds.reserve(trackFeaturesIds.results.size());

        LMS_LOG(RECOMMENDATION, DEBUG, "Extracting features...");
        // TODO handle errors using exceptions
        for (const TrackFeaturesId trackFeaturesId : trackFeaturesIds.results)
        {
            if (_loadCancelled)
                return;

            auto transaction{ session.createReadTransaction() };

            TrackFeatures::pointer trackFeatures{ TrackFeatures::find(session, trackFeaturesId) };
            if (!trackFeatures)
                continue;

            FeatureValuesMap featureValuesMap{ trackFeatures->getFeatureValuesMap(featureNames) };
            if (featureValuesMap.empty())
                continue;

            std::optional<som::InputVector> inputVector{ convertFeatureValuesMapToInputVector(featureValuesMap, nbDimensions) };
            if (!inputVector)
                continue;

            samples.emplace_back(std::move(*inputVector));
            samplesTrackIds.emplace_back(trackFeatures->getTrack()->getId());
        }
        LMS_LOG(RECOMMENDATION, DEBUG, "Extracting features DONE");

        if (samples.empty())
        {
            LMS_LOG(RECOMMENDATION, INFO, "Nothing to classify!");
            return;
        }

        LMS_LOG(RECOMMENDATION, DEBUG, "Normalizing data...");
        som::DataNormalizer dataNormalizer{ nbDimensions };

        dataNormalizer.computeNormalizationFactors(samples);
        for (auto& sample : samples)
            dataNormalizer.normalizeData(sample);

        som::Coordinate size{ static_cast<som::Coordinate>(std::sqrt(samples.size() / trainSettings.sampleCountPerNeuron)) };
        if (size < 2)
        {
            LMS_LOG(RECOMMENDATION, WARNING, "Very few tracks (" << samples.size() << ") are being used by the features engine, expect bad behaviors");
            size = 2;
        }
        LMS_LOG(RECOMMENDATION, INFO, "Found " << samples.size() << " tracks, constructing a " << size << "*" << size << " network");

        som::Network network{ size, size, nbDimensions };

        som::InputVector weights{ getInputVectorWeights(trainSettings.featureSettingsMap, nbDimensions) };
        network.setDataWeights(weights);

        auto somProgressCallback{ [&](const som::Network::CurrentIteration& iter) {
            LMS_LOG(RECOMMENDATION, DEBUG, "Current pass = " << iter.idIteration << " / " << iter.iterationCount);
            progressCallback(Progress{ iter.idIteration, iter.iterationCount });
        } };

        LMS_LOG(RECOMMENDATION, DEBUG, "Training network...");
        network.train(samples, trainSettings.iterationCount,
            progressCallback ? somProgressCallback : som::Network::ProgressCallback{},
            [this] { return _loadCancelled; });
        LMS_LOG(RECOMMENDATION, DEBUG, "Training network DONE");

        LMS_LOG(RECOMMENDATION, DEBUG, "Classifying tracks...");
        TrackPositions trackPositions;
        for (std::size_t i{}; i < samples.size(); ++i)
        {
            if (_loadCancelled)
                return;

            const som::Position position{ network.getClosestRefVectorPosition(samples[i]) };

            trackPositions[samplesTrackIds[i]].push_back(position);
        }

        LMS_LOG(RECOMMENDATION, DEBUG, "Classifying tracks DONE");

        load(std::move(network), std::move(trackPositions));
    }

    void FeaturesEngine::loadFromCache(FeaturesEngineCache&& cache)
    {
        LMS_LOG(RECOMMENDATION, INFO, "Constructing features classifier from cache...");

        load(std::move(cache._network), cache._trackPositions);
    }

    TrackContainer FeaturesEngine::findSimilarTracksFromTrackList(TrackListId trackListId, std::size_t maxCount) const
    {
        const TrackContainer trackIds{ [&] {
            TrackContainer res;

            Session& session{ _db.getTLSSession() };

            auto transaction{ session.createReadTransaction() };

            const TrackList::pointer trackList{ TrackList::find(session, trackListId) };
            if (trackList)
                res = trackList->getTrackIds();

            return res;
        }() };

        return findSimilarTracks(trackIds, maxCount);
    }

    TrackContainer FeaturesEngine::findSimilarTracks(const std::vector<TrackId>& tracksIds, std::size_t maxCount) const
    {
        auto similarTrackIds{ getSimilarObjects(tracksIds, _trackMatrix, _trackPositions, maxCount) };

        Session& session{ _db.getTLSSession() };

        {
            // Report only existing ids, as tracks may have been removed a long time ago (refreshing the SOM takes some time)
            auto transaction{ session.createReadTransaction() };

            similarTrackIds.erase(std::remove_if(std::begin(similarTrackIds), std::end(similarTrackIds),
                                      [&](TrackId trackId) {
                                          return !Track::exists(session, trackId);
                                      }),
                std::end(similarTrackIds));
        }

        return similarTrackIds;
    }

    ReleaseContainer FeaturesEngine::getSimilarReleases(ReleaseId releaseId, std::size_t maxCount) const
    {
        auto similarReleaseIds{ getSimilarObjects({ releaseId }, _releaseMatrix, _releasePositions, maxCount) };

        Session& session{ _db.getTLSSession() };

        if (!similarReleaseIds.empty())
        {
            // Report only existing ids
            auto transaction{ session.createReadTransaction() };

            similarReleaseIds.erase(std::remove_if(std::begin(similarReleaseIds), std::end(similarReleaseIds),
                                        [&](ReleaseId releaseId) {
                                            return !Release::exists(session, releaseId);
                                        }),
                std::end(similarReleaseIds));
        }

        return similarReleaseIds;
    }

    ArtistContainer FeaturesEngine::getSimilarArtists(ArtistId artistId, core::EnumSet<TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        auto getSimilarArtistIdsForLinkType{ [&](TrackArtistLinkType linkType) {
            ArtistContainer similarArtistIds;

            const auto itArtists{ _artistMatrix.find(linkType) };
            if (itArtists == std::cend(_artistMatrix))
            {
                return similarArtistIds;
            }

            return getSimilarObjects({ artistId }, itArtists->second, _artistPositions, maxCount);
        } };

        std::unordered_set<ArtistId> similarArtistIds;

        for (TrackArtistLinkType linkType : linkTypes)
        {
            const auto similarArtistIdsForLinkType{ getSimilarArtistIdsForLinkType(linkType) };
            similarArtistIds.insert(std::begin(similarArtistIdsForLinkType), std::end(similarArtistIdsForLinkType));
        }

        ArtistContainer res(std::cbegin(similarArtistIds), std::cend(similarArtistIds));

        Session& session{ _db.getTLSSession() };
        {
            // Report only existing ids
            auto transaction{ session.createReadTransaction() };

            res.erase(std::remove_if(std::begin(res), std::end(res),
                          [&](ArtistId artistId) {
                              return !Artist::exists(session, artistId);
                          }),
                std::end(res));
        }

        while (res.size() > maxCount)
            res.erase(core::random::pickRandom(res));

        return res;
    }

    FeaturesEngineCache FeaturesEngine::toCache() const
    {
        return FeaturesEngineCache{ *_network, _trackPositions };
    }

    void FeaturesEngine::load(bool forceReload, const ProgressCallback& progressCallback)
    {
        if (forceReload)
        {
            FeaturesEngineCache::invalidate();
        }
        else if (std::optional<FeaturesEngineCache> cache{ FeaturesEngineCache::read() })
        {
            loadFromCache(std::move(*cache));
            return;
        }

        TrainSettings trainSettings;
        trainSettings.featureSettingsMap = getDefaultTrainFeatureSettings();

        loadFromTraining(trainSettings, progressCallback);
        if (!_loadCancelled && _network)
            toCache().write();
    }

    void FeaturesEngine::requestCancelLoad()
    {
        LMS_LOG(RECOMMENDATION, DEBUG, "Requesting init cancellation");
        _loadCancelled = true;
    }

    void FeaturesEngine::load(const som::Network& network, const TrackPositions& trackPositions)
    {
        using namespace db;

        _networkRefVectorsDistanceMedian = network.computeRefVectorsDistanceMedian();
        LMS_LOG(RECOMMENDATION, DEBUG, "Median distance betweend ref vectors = " << _networkRefVectorsDistanceMedian);

        const som::Coordinate width{ network.getWidth() };
        const som::Coordinate height{ network.getHeight() };

        _releaseMatrix = ReleaseMatrix{ width, height };
        _trackMatrix = TrackMatrix{ width, height };

        LMS_LOG(RECOMMENDATION, DEBUG, "Constructing maps...");

        Session& session{ _db.getTLSSession() };

        for (const auto& [trackId, positions] : trackPositions)
        {
            if (_loadCancelled)
                return;

            auto transaction{ session.createReadTransaction() };

            const Track::pointer track{ Track::find(session, trackId) };
            if (!track)
                continue;

            for (const som::Position& position : positions)
            {
                core::utils::push_back_if_not_present(_trackPositions[trackId], position);
                core::utils::push_back_if_not_present(_trackMatrix[position], trackId);

                if (Release::pointer release{ track->getRelease() })
                {
                    const ReleaseId releaseId{ release->getId() };
                    core::utils::push_back_if_not_present(_releasePositions[releaseId], position);
                    core::utils::push_back_if_not_present(_releaseMatrix[position], releaseId);
                }
                for (const TrackArtistLink::pointer& artistLink : track->getArtistLinks())
                {
                    const ArtistId artistId{ artistLink->getArtist()->getId() };

                    core::utils::push_back_if_not_present(_artistPositions[artistId], position);
                    auto itArtists{ _artistMatrix.find(artistLink->getType()) };
                    if (itArtists == std::cend(_artistMatrix))
                    {
                        [[maybe_unused]] auto [it, inserted] = _artistMatrix.try_emplace(artistLink->getType(), ArtistMatrix{ width, height });
                        assert(inserted);
                        itArtists = it;
                    }
                    core::utils::push_back_if_not_present(itArtists->second[position], artistId);
                }
            }
        }

        _network = std::make_unique<som::Network>(network);

        LMS_LOG(RECOMMENDATION, INFO, "Classifier successfully loaded!");
    }

} // namespace lms::recommendation
