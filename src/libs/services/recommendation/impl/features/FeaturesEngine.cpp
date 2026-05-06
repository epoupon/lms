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

#include <algorithm>
#include <array>
#include <memory>
#include <unordered_map>
#include <utility>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "audio/AudioFeatures.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackAudioFeatures.hpp"
#include "math/CentroidCalculator.hpp"
#include "math/CovarianceCalculator.hpp"
#include "math/PrincipalComponents.hpp"
#include "math/StatsAccumulator.hpp"

#include "features/FeaturesDefs.hpp"

namespace lms::recommendation
{
    namespace
    {
        void flattenAudioFeatures(const audio::AudioFeatures& rawFeatures, AudioFeatureVector& audioFeatures)
        {
            std::size_t featureIndex{};

            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.logMelEnergyStdDev[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.logMelEnergyMean[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.logMelEnergyDeltaStdDev[i];
            for (std::size_t i{}; i < audio::AudioFeatures::melBandCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.logMelEnergyDeltaMeanAbs[i];

            for (std::size_t i{}; i < audio::AudioFeatures::mfccCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.mfccMean[i];
            for (std::size_t i{}; i < audio::AudioFeatures::mfccCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.mfccStdDev[i];
            for (std::size_t i{}; i < audio::AudioFeatures::mfccCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.mfccDeltaMeanAbs[i];
            for (std::size_t i{}; i < audio::AudioFeatures::mfccCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.mfccDeltaStdDev[i];

            audioFeatures[featureIndex++] = rawFeatures.spectralCentroidMean;
            audioFeatures[featureIndex++] = rawFeatures.spectralCentroidStdDev;
            audioFeatures[featureIndex++] = rawFeatures.spectralCentroidDeltaMeanAbs;
            audioFeatures[featureIndex++] = rawFeatures.spectralCentroidDeltaStdDev;

            audioFeatures[featureIndex++] = rawFeatures.spectralRolloffMean;
            audioFeatures[featureIndex++] = rawFeatures.spectralRolloffStdDev;
            audioFeatures[featureIndex++] = rawFeatures.spectralRolloffDeltaMeanAbs;
            audioFeatures[featureIndex++] = rawFeatures.spectralRolloffDeltaStdDev;

            audioFeatures[featureIndex++] = rawFeatures.spectralFluxMean;
            audioFeatures[featureIndex++] = rawFeatures.spectralFluxStdDev;
            audioFeatures[featureIndex++] = rawFeatures.spectralFluxDeltaMeanAbs;

            for (std::size_t i{}; i < audio::AudioFeatures::chromaCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.chromaMean[i];
            for (std::size_t i{}; i < audio::AudioFeatures::chromaCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.chromaStdDev[i];
            for (std::size_t i{}; i < audio::AudioFeatures::chromaCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.chromaDeltaMeanAbs[i];
            for (std::size_t i{}; i < audio::AudioFeatures::chromaCount; ++i)
                audioFeatures[featureIndex++] = rawFeatures.chromaDeltaStdDev[i];

            audioFeatures[featureIndex++] = rawFeatures.zeroCrossingRateMean;
            audioFeatures[featureIndex++] = rawFeatures.zeroCrossingRateStdDev;

            assert(featureIndex == audioFeatureCount);
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
        _ioContext.post([this] { reload(); });
    }

    TrackContainer FeaturesEngine::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        return {};
    }

    TrackContainer FeaturesEngine::findSimilarTracks(const std::vector<db::TrackId>& tracksId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find similar tracks");

        TrackContainer res;
        if (maxCount == 0 || tracksId.empty() || !_pcaReady)
            return res;

        math::CentroidCalculator<ReducedFeatureVector> centroidCalculator;
        for (const db::TrackId trackId : tracksId)
        {
            const auto it{ _reducedFeatures.find(trackId) };
            if (it == _reducedFeatures.cend())
                continue;

            centroidCalculator.add(it->second);
        }

        if (centroidCalculator.empty())
            return res;

        const ReducedFeatureVector queryVector{ centroidCalculator.finalizeNormalized() };

        using Distance = float;
        std::vector<std::pair<db::TrackId, Distance>> rankedTracks;
        rankedTracks.reserve(_reducedFeatures.size());

        for (const auto& [trackId, features] : _reducedFeatures)
        {
            if (std::find(std::cbegin(tracksId), std::cend(tracksId), trackId) != std::cend(tracksId))
                continue;

            rankedTracks.emplace_back(trackId, queryVector.computeEuclideanSquaredDistance(features));
        }

        const std::size_t resultCount{ std::min(maxCount, rankedTracks.size()) };
        std::partial_sort(std::begin(rankedTracks), std::next(std::begin(rankedTracks), resultCount), std::end(rankedTracks), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back(rankedTracks[i].first);

        return res;
    }

    ReleaseContainer FeaturesEngine::getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find similar releases");

        ReleaseContainer res;
        if (maxCount == 0 || !_pcaReady)
            return res;

        const auto it{ _releaseCentroids.find(releaseId) };
        if (it == _releaseCentroids.cend())
            return res;

        const ReducedFeatureVector& queryVector{ it->second };

        using Distance = float;
        std::vector<std::pair<db::ReleaseId, Distance>> rankedReleases;
        rankedReleases.reserve(_releaseCentroids.size());

        for (const auto& [candidateId, centroid] : _releaseCentroids)
        {
            if (candidateId == releaseId)
                continue;

            rankedReleases.emplace_back(candidateId, queryVector.computeEuclideanSquaredDistance(centroid));
        }

        const std::size_t resultCount{ std::min(maxCount, rankedReleases.size()) };
        std::partial_sort(std::begin(rankedReleases), std::next(std::begin(rankedReleases), resultCount), std::end(rankedReleases), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back(rankedReleases[i].first);

        return res;
    }

    ArtistContainer FeaturesEngine::getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find similar artists");

        ArtistContainer res;
        if (maxCount == 0 || !_pcaReady)
            return res;

        const auto it{ _artistCentroids.find(artistId) };
        if (it == _artistCentroids.cend())
            return res;

        const ReducedFeatureVector& queryVector{ it->second };

        using Distance = float;
        std::vector<std::pair<db::ArtistId, Distance>> rankedArtists;
        rankedArtists.reserve(_artistCentroids.size());

        for (const auto& [candidateId, centroid] : _artistCentroids)
        {
            if (candidateId == artistId)
                continue;

            if (!linkTypes.empty())
            {
                const auto linkIt{ _artistLinkTypes.find(candidateId) };
                if (linkIt == _artistLinkTypes.cend() || !(linkIt->second.getBitfield() & linkTypes.getBitfield()))
                    continue;
            }

            rankedArtists.emplace_back(candidateId, queryVector.computeEuclideanSquaredDistance(centroid));
        }

        const std::size_t resultCount{ std::min(maxCount, rankedArtists.size()) };
        std::partial_sort(std::begin(rankedArtists), std::next(std::begin(rankedArtists), resultCount), std::end(rankedArtists), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back(rankedArtists[i].first);

        return res;
    }

    void FeaturesEngine::abort()
    {
        _abortRequested = true;
        _ioContextRunner.wait();
    }

    void FeaturesEngine::reload()
    {
        LMS_SCOPED_TRACE_OVERVIEW("FeaturesEngine", "Loading");

        LMS_LOG(RECOMMENDATION, INFO, "Loading...");
        computeDatasetStats();
        computeReducedFeatures();
        LMS_LOG(RECOMMENDATION, INFO, "Loading complete!");
    }

    void FeaturesEngine::computeDatasetStats()
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Compute dataset stats");

        using AudioFeatureMatrix = math::SquareMatrix<FloatType, audioFeatureCount>;

        LMS_LOG(RECOMMENDATION, DEBUG, "Computing dataset stats...");

        _trackCount = 0;
        _pcaReady = false;
        AudioFeatureVector audioFeatures; // cache
        std::array<math::StatsAccumulator<audio::FeatureValue>, audioFeatureCount> statsAccumulators;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
                getAudioFeatureVector(features, audioFeatures);
                for (std::size_t i{}; i < audioFeatureCount; ++i)
                    statsAccumulators[i].add(audioFeatures[i]);

                _trackCount++;
            });
        }

        for (std::size_t featureIndex{}; featureIndex < audioFeatureCount; ++featureIndex)
            _featureMeans[featureIndex] = static_cast<audio::FeatureValue>(statsAccumulators[featureIndex].getMean());

        // Compute covariance
        const AudioFeatureMatrix covariance = [&]() {
            math::CovarianceMatrixCalculator<audioFeatureCount, FloatType> calculator;

            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
                getAudioFeatureVector(features, audioFeatures);

                for (std::size_t i{}; i < audioFeatureCount; ++i)
                    audioFeatures[i] -= _featureMeans[i];

                calculator.add(audioFeatures);
            });

            return calculator.finalizeSample();
        }();

        // PCA via power iteration + deflation in double precision
        {
            using EigenMatrix = math::SquareMatrix<double, audioFeatureCount>;
            using EigenVector = math::Vector<audioFeatureCount, double>;
            EigenMatrix covCopy;
            for (std::size_t i{}; i < audioFeatureCount; ++i)
            {
                for (std::size_t j{}; j < audioFeatureCount; ++j)
                    covCopy[i][j] = static_cast<double>(covariance[i][j]);
            }

            EigenVector eigenvalues{};
            std::array<EigenVector, audioFeatureCount> eigenvectors;

            math::computeEigenpairsViaPowerIteration(covCopy, eigenvectors, eigenvalues);

            // Store PCA basis and whitening scales (top pcaDimCount only)
            for (std::size_t k{}; k < pcaDimCount; ++k)
            {
                for (std::size_t j{}; j < audioFeatureCount; ++j)
                    _pcaBasis[k][j] = static_cast<FloatType>(eigenvectors[k][j]);

                _pcaScale[k] = (eigenvalues[k] > 1e-15) ? static_cast<FloatType>(1.0 / std::sqrt(eigenvalues[k])) : FloatType{};
            }

            _pcaReady = true;
        }

        LMS_LOG(RECOMMENDATION, DEBUG, "Computing dataset stats DONE");
    }

    void FeaturesEngine::getAudioFeatureVector(const db::ObjectPtr<db::TrackAudioFeatures>& features, AudioFeatureVector& inputVector)
    {
        audio::AudioFeatures audioFeatures;
        audio::audioFeaturesFromBlob(features->getData(), audioFeatures);
        flattenAudioFeatures(audioFeatures, inputVector);
    }

    void FeaturesEngine::getReducedFeatureVector(const db::ObjectPtr<db::TrackAudioFeatures>& features, ReducedFeatureVector& output) const
    {
        AudioFeatureVector inputVector;
        getAudioFeatureVector(features, inputVector);
        for (std::size_t i{}; i < audioFeatureCount; ++i)
            inputVector[i] -= _featureMeans[i];

        projectToReduced(inputVector, output);
        output.normalizeL2();
    }

    void FeaturesEngine::projectToReduced(const AudioFeatureVector& centered, ReducedFeatureVector& output) const
    {
        if (!_pcaReady)
            return;

        math::projectOntoBasis(_pcaBasis, centered, output, _pcaScale);
    }

    void FeaturesEngine::computeReducedFeatures()
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "ComputeReducedFeatures");

        LMS_LOG(RECOMMENDATION, DEBUG, "Computing " << pcaDimCount << " reduced dimensions from " << audioFeatureCount << " audio features...");

        _reducedFeatures.clear();

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        ReducedFeatureVector reducedVector;
        db::TrackAudioFeatures::find(session, [&](const db::TrackAudioFeatures::pointer& features) {
            getReducedFeatureVector(features, reducedVector);
            _reducedFeatures.emplace(features->getTrackId(), reducedVector);
        });

        LMS_LOG(RECOMMENDATION, DEBUG, "Cached " << _reducedFeatures.size() << " reduced feature vectors in memory");

        // Compute release centroids and artist centroids
        _releaseCentroids.clear();
        _artistCentroids.clear();
        _artistLinkTypes.clear();
        std::unordered_map<db::ReleaseId, math::CentroidCalculator<ReducedFeatureVector>> releaseCentroidCalculators;
        std::unordered_map<db::ArtistId, math::CentroidCalculator<ReducedFeatureVector>> artistCentroidCalculators;

        for (const auto& [trackId, reducedVec] : _reducedFeatures)
        {
            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (!track)
                continue;

            const db::ReleaseId relId{ track->getReleaseId() };
            if (relId.isValid())
            {
                releaseCentroidCalculators[relId].add(reducedVec);

                // Release-level artist links (considered as Artist link type)
                const db::Release::pointer release{ db::Release::find(session, relId) };
                if (release)
                {
                    for (const auto& link : release->getArtistLinks())
                    {
                        const db::ArtistId artistId{ link->getArtistId() };
                        artistCentroidCalculators[artistId].add(reducedVec);
                        _artistLinkTypes[artistId].insert(db::TrackArtistLinkType::Artist);
                    }
                }
            }

            // Track-level artist links
            for (const auto& link : track->getArtistLinks())
            {
                const db::ArtistId artistId{ link->getArtistId() };
                artistCentroidCalculators[artistId].add(reducedVec);
                _artistLinkTypes[artistId].insert(link->getType());
            }
        }

        for (auto& [relId, calculator] : releaseCentroidCalculators)
            _releaseCentroids[relId] = calculator.finalizeNormalized();

        for (auto& [artistId, calculator] : artistCentroidCalculators)
            _artistCentroids[artistId] = calculator.finalizeNormalized();

        LMS_LOG(RECOMMENDATION, DEBUG, "Computed " << _releaseCentroids.size() << " release centroids");
        LMS_LOG(RECOMMENDATION, DEBUG, "Computed " << _artistCentroids.size() << " artist centroids");
        LMS_LOG(RECOMMENDATION, DEBUG, "Computing reduced features DONE");
    }

    void FeaturesEngine::computeReleaseHitRank()
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        constexpr std::size_t maxSimilarTrackCount{ 250 };

        LMS_LOG(RECOMMENDATION, DEBUG, "Getting all features");

        struct TrackDesc
        {
            db::ReleaseId releaseId;
            const ReducedFeatureVector* features{};
        };
        std::unordered_map<db::TrackId, TrackDesc> trackDescs;

        for (const auto& [trackId, features] : _reducedFeatures)
        {
            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (!track)
                continue;

            const db::ReleaseId releaseId{ track->getReleaseId() };
            if (!releaseId.isValid())
                continue;

            {
                db::Track::FindParameters params;
                params.setRelease(releaseId);
                const auto releaseTrackIds{ db::Track::findIds(session, params) };
                if (releaseTrackIds.results.size() == 1)
                    continue;
            }

            trackDescs[trackId] = TrackDesc{ .releaseId = releaseId, .features = &features };
        }

        std::array<std::atomic<std::size_t>, maxSimilarTrackCount + 1> ranks{};

        LMS_LOG(RECOMMENDATION, DEBUG, "processing " << trackDescs.size() << " tracks using " << std::thread::hardware_concurrency() << " threads");
        std::atomic<std::size_t> i;

        std::vector<std::unique_ptr<std::thread>> threads;

        for (std::size_t t = 0; t < std::thread::hardware_concurrency(); ++t)
        {
            threads.push_back(std::make_unique<std::thread>([&]() {
                std::vector<db::TrackId> sortedTrackIds;

                while (true)
                {
                    const std::size_t index = i++;
                    if (index >= trackDescs.size())
                        break;

                    const auto itEntry{ std::next(trackDescs.cbegin(), index) };
                    const TrackDesc& trackDesc = itEntry->second;

                    sortedTrackIds.clear();
                    for (const auto& [trackId, desc] : trackDescs)
                    {
                        if (trackId != itEntry->first)
                            sortedTrackIds.push_back(trackId);
                    }

                    if (index % 1'000 == 0)
                        LMS_LOG(RECOMMENDATION, DEBUG, "Processing " << index << "th track... rank 0 = " << ranks[0] << ", out of rank = " << ranks[maxSimilarTrackCount]);

                    math::SquaredEuclideanDistance dist{ *trackDesc.features };
                    std::sort(sortedTrackIds.begin(), sortedTrackIds.end(), [&](db::TrackId trackA, db::TrackId trackB) {
                        return dist(*trackDescs[trackA].features) < dist(*trackDescs[trackB].features);
                    });

                    bool found{};
                    for (std::size_t i{}; i < maxSimilarTrackCount; ++i)
                    {
                        const auto itTrackDesc{ trackDescs.find(sortedTrackIds[i]) };
                        if (itTrackDesc == std::cend(trackDescs))
                        {
                            LMS_LOG(RECOMMENDATION, DEBUG, "??");
                            continue;
                        }

                        if (itTrackDesc->second.releaseId == trackDesc.releaseId)
                        {
                            ranks[i]++;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        ranks[maxSimilarTrackCount]++;
                }

                return 0;
            }));
        }

        for (auto& t : threads)
            t->join();

        LMS_LOG(RECOMMENDATION, DEBUG, "Release hit ranks computed using kNN");
        for (std::size_t i{}; i < maxSimilarTrackCount; ++i)
            LMS_LOG(RECOMMENDATION, DEBUG, "Rank " << i << ": " << ranks[i]);

        LMS_LOG(RECOMMENDATION, DEBUG, "Too far rank: " << ranks[maxSimilarTrackCount]);
    }
} // namespace lms::recommendation
