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

#pragma once

#include "AudioSimilarityEngine.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <utility>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "math/ChamferDistance.hpp"
#include "math/CovarianceCalculator.hpp"
#include "math/MedoidCalculator.hpp"
#include "math/NormalizedCosineDistance.hpp"
#include "math/PrincipalComponents.hpp"
#include "math/StatsAccumulator.hpp"

#include "track-selection-constraints/DuplicateTrackConstraint.hpp"
#include "track-selection-constraints/InterpolationFitConstraint.hpp"
#include "track-selection-constraints/SmoothTransitionConstraint.hpp"

#include "Types.hpp"

namespace lms::recommendation
{
    namespace detail
    {
        template<typename ReducedVector>
        TrackResults findNearestNeighbors(
            const ReducedVector& queryVector, // expected to be normalized
            const std::unordered_map<db::TrackId, const ReducedVector*>& trackVectors,
            std::size_t maxNeighbors,
            db::TrackId excludeTrackId)
        {
            const math::NormalizedCosineDistance distFunc{ queryVector };

            TrackResults neighbors;
            neighbors.reserve(trackVectors.size());

            for (const auto& [trackId, trackVector] : trackVectors)
            {
                if (trackId == excludeTrackId)
                    continue;

                neighbors.push_back({ .id = trackId, .score = distFunc(*trackVector) });
            }

            maxNeighbors = std::min(maxNeighbors, neighbors.size());
            if (maxNeighbors == 0)
                return {};

            std::nth_element(neighbors.begin(), neighbors.begin() + static_cast<std::ptrdiff_t>(maxNeighbors), neighbors.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.score < rhs.score;
            });
            neighbors.resize(maxNeighbors);
            std::sort(neighbors.begin(), neighbors.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.score < rhs.score;
            });
            return neighbors;
        }
    } // namespace detail

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    AudioSimilarityEngine<Provider, ReducedDimCount>::AudioSimilarityEngine(db::IDb& db)
        : _db{ db }
        , _ioContextRunner{ _ioContext, 1, "FeaturesEngine" }
    {
        initializeConstraints();
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    AudioSimilarityEngine<Provider, ReducedDimCount>::~AudioSimilarityEngine()
    {
        abort();
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::requestReload()
    {
        _isReady = false;
        _ioContext.post([this] { reload(); });
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>

    bool AudioSimilarityEngine<Provider, ReducedDimCount>::isLoaded() const
    {
        return _isReady;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::initializeConstraints()
    {
        _trackCandidateEvaluator = {};

        _trackCandidateEvaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
        _trackCandidateEvaluator.addSoftConstraint(std::make_unique<InterpolationFitConstraint>(), 0.8F);
        _trackCandidateEvaluator.addSoftConstraint(std::make_unique<SmoothTransitionConstraint>(), 0.2F);
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>

    TrackResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        return {};
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>

    TrackResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarTracks(std::span<const db::TrackId> tracksId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find similar tracks");

        TrackResults res;
        if (maxCount == 0 || tracksId.empty() || !_isReady)
            return res;

        math::MedoidCalculator<ReducedVector> medoidCalculator;
        for (const db::TrackId trackId : tracksId)
        {
            const auto it{ _trackVectors.find(trackId) };
            if (it == _trackVectors.cend())
                continue;

            medoidCalculator.add(*it->second);
        }

        if (medoidCalculator.empty())
            return res;

        const ReducedVector queryVector{ medoidCalculator.finalize() };
        const math::NormalizedCosineDistance distFunc{ queryVector };

        using Distance = float;
        std::vector<std::pair<db::TrackId, Distance>> rankedTracks;
        rankedTracks.reserve(_trackVectors.size());

        for (const auto& [trackId, vectors] : _trackVectors)
        {
            if (std::find(std::cbegin(tracksId), std::cend(tracksId), trackId) != std::cend(tracksId))
                continue;

            rankedTracks.emplace_back(trackId, distFunc(*vectors));
        }

        const std::size_t resultCount{ std::min(maxCount, rankedTracks.size()) };
        std::partial_sort(std::begin(rankedTracks), std::next(std::begin(rankedTracks), resultCount), std::end(rankedTracks), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back({ .id = rankedTracks[i].first, .score = rankedTracks[i].second });

        return res;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    TrackResults AudioSimilarityEngine<Provider, ReducedDimCount>::findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find track similarity path");

        if (maxCount == 0 || !_isReady)
            return {};

        const auto itStart{ _trackVectors.find(startTrackId) };
        const auto itEnd{ _trackVectors.find(endTrackId) };
        if (itStart == _trackVectors.cend() || itEnd == _trackVectors.cend())
            return {};

        const ReducedVector startVector{ *itStart->second };
        const ReducedVector endVector{ *itEnd->second };
        const ReducedVector direction{ endVector - startVector };

        std::vector<db::TrackId> path;
        path.reserve(maxCount);
        path.push_back(startTrackId);

        const ReducedVector* previousVector{ itStart->second };
        static constexpr std::size_t DefaultNeighborCount{ 16 };
        static constexpr std::size_t BroadNeighborCount{ 64 };
        std::size_t neighborCount{ DefaultNeighborCount };
        const std::size_t interiorCount{ (maxCount > 2) ? (maxCount - 2) : 0 };

        auto evaluateCandidates = [&](const auto& neighborList) -> std::optional<db::TrackId> {
            std::optional<db::TrackId> best;
            float bestScore{ std::numeric_limits<float>::max() };

            for (const auto& [candidateId, candidateDistance] : neighborList)
            {
                const auto* candidateVector{ _trackVectors.at(candidateId) };
                const float transitionDistance{ math::NormalizedCosineDistance{ *previousVector }(*candidateVector) };

                const TrackCandidateContext context{
                    .candidateTrackId = candidateId,
                    .selectedTracks = path,
                    .distanceToQuery = candidateDistance,
                    .distanceToPrevious = transitionDistance,
                };

                if (_trackCandidateEvaluator.rejects(context))
                    continue;

                const float score{ _trackCandidateEvaluator.score(context) };
                if (score < bestScore)
                {
                    bestScore = score;
                    best = candidateId;
                }
            }

            return best;
        };

        for (std::size_t i{}; i < interiorCount; ++i)
        {
            const float t{ static_cast<float>(i + 1) / static_cast<float>(interiorCount + 1) };
            auto queryPoint{ startVector + direction * t };
            queryPoint.normalizeL2();

            const auto neighbors{ detail::findNearestNeighbors(queryPoint, _trackVectors, neighborCount, endTrackId) };
            std::optional<db::TrackId> bestCandidate{ evaluateCandidates(neighbors) };

            if (!bestCandidate && neighborCount < BroadNeighborCount)
            {
                neighborCount = BroadNeighborCount;
                const auto broaderNeighbors{ detail::findNearestNeighbors(queryPoint, _trackVectors, neighborCount, endTrackId) };
                bestCandidate = evaluateCandidates(broaderNeighbors);
            }

            if (!bestCandidate)
                continue;

            path.push_back(*bestCandidate);
            previousVector = _trackVectors.at(*bestCandidate);
        }

        if (maxCount > 1)
            path.push_back(endTrackId);

        TrackResults results;
        results.reserve(path.size());

        float cumulativeCost{};
        for (std::size_t i{}; i < path.size(); ++i)
        {
            const db::TrackId trackId{ path[i] };
            if (i == 0)
            {
                results.push_back({ .id = trackId, .score = cumulativeCost });
                continue;
            }

            const auto* previousVector{ _trackVectors.at(path[i - 1]) };
            const auto* currentVector{ _trackVectors.at(trackId) };
            cumulativeCost += math::NormalizedCosineDistance{ *currentVector }(*previousVector);
            results.push_back({ .id = trackId, .score = cumulativeCost });
        }

        return results;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    ReleaseResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarReleases(
        db::ReleaseId releaseId,
        std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find similar releases");

        ResultContainer<db::ReleaseId> res;
        if (maxCount == 0 || !_isReady)
            return res;

        const auto itQueryRelease{ _releaseVectors.find(releaseId) };
        if (itQueryRelease == _releaseVectors.cend() || itQueryRelease->second.empty())
            return res;

        const auto& queryReleaseFeatures{ itQueryRelease->second };

        using Distance = float;
        std::vector<std::pair<db::ReleaseId, Distance>> rankedReleases;
        rankedReleases.reserve(_releaseVectors.size());

        using CosineDistance = math::NormalizedCosineDistance<ReducedVector::getSize(), FloatType>;

        for (const auto& [candidateId, candidateReleaseVectors] : _releaseVectors)
        {
            if (candidateId == releaseId || candidateReleaseVectors.empty())
                continue;

            const FloatType distance{ math::symmetricalChamferDistance<CosineDistance>(
                queryReleaseFeatures,
                candidateReleaseVectors) };

            rankedReleases.emplace_back(candidateId, distance);
        }

        const std::size_t resultCount{ std::min(maxCount, rankedReleases.size()) };
        std::partial_sort(std::begin(rankedReleases), std::next(std::begin(rankedReleases), resultCount), std::end(rankedReleases), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back({ .id = rankedReleases[i].first, .score = rankedReleases[i].second });

        return res;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    ArtistResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Find similar artists");

        ArtistResults res;
        if (maxCount == 0 || !_isReady)
            return res;

        if (!linkTypes.contains(db::TrackArtistLinkType::Artist))
            return res;

        const auto itQueryArtist{ _artistVectors.find(artistId) };
        if (itQueryArtist == _artistVectors.cend() || itQueryArtist->second.empty())
            return res;

        const auto& queryArtistFeatures{ itQueryArtist->second };

        using Distance = float;
        std::vector<std::pair<db::ArtistId, Distance>> rankedArtists;
        rankedArtists.reserve(_artistVectors.size());

        using CosineDistance = math::NormalizedCosineDistance<ReducedVector::getSize(), typename ReducedVector::value_type>;

        for (const auto& [candidateId, candidateArtistFeatures] : _artistVectors)
        {
            if (candidateId == artistId || candidateArtistFeatures.empty())
                continue;

            const FloatType distance{ math::symmetricalChamferDistance<CosineDistance>(
                queryArtistFeatures,
                candidateArtistFeatures) };

            rankedArtists.emplace_back(candidateId, distance);
        }

        const std::size_t resultCount{ std::min(maxCount, rankedArtists.size()) };
        std::partial_sort(std::begin(rankedArtists), std::next(std::begin(rankedArtists), resultCount), std::end(rankedArtists), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back({ .id = rankedArtists[i].first, .score = rankedArtists[i].second });

        return res;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::abort()
    {
        _abortRequested = true;
        _ioContextRunner.wait();
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::reload()
    {
        LMS_SCOPED_TRACE_OVERVIEW("FeaturesEngine", "Loading");

        LMS_LOG(RECOMMENDATION, INFO, "Loading...");
        computeDatasetStats();
        computeReducedFeatures();
        _isReady = true;
        LMS_LOG(RECOMMENDATION, INFO, "Loading complete!");
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeDatasetStats()
    {
        LMS_SCOPED_TRACE_DETAILED("FeaturesEngine", "Compute dataset stats");

        using AudioFeatureMatrix = math::SquareMatrix<FloatType, SourceDimCount>;

        LMS_LOG(RECOMMENDATION, DEBUG, "Computing dataset stats...");

        _pcaReady = false;
        _trackCount = 0;
        SourceVector sourceVector; // cache
        std::array<math::StatsAccumulator<FloatType>, SourceDimCount> statsAccumulators;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            Provider::visitVectors(session, [&](db::TrackId trackId, const SourceVector& sourceVector) {
                for (std::size_t i{}; i < SourceDimCount; ++i)
                    statsAccumulators[i].add(sourceVector[i]);

                _trackCount++;
            });
        }

        for (std::size_t featureIndex{}; featureIndex < SourceDimCount; ++featureIndex)
            _sourceMeans[featureIndex] = static_cast<FloatType>(statsAccumulators[featureIndex].getMean());

        // Compute covariance
        const AudioFeatureMatrix covariance = [&]() {
            math::CovarianceMatrixCalculator<SourceDimCount, FloatType> calculator;

            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            Provider::visitVectors(session, [&](db::TrackId, SourceVector& sourceVector) {
                for (std::size_t i{}; i < SourceDimCount; ++i)
                    sourceVector[i] -= _sourceMeans[i];

                calculator.add(sourceVector);
            });

            return calculator.finalizeSample();
        }();

        // PCA via power iteration + deflation in double precision
        {
            using EigenMatrix = math::SquareMatrix<double, SourceDimCount>;
            using EigenVector = math::Vector<SourceDimCount, double>;
            EigenMatrix covCopy;
            for (std::size_t i{}; i < SourceDimCount; ++i)
            {
                for (std::size_t j{}; j < SourceDimCount; ++j)
                    covCopy[i][j] = static_cast<double>(covariance[i][j]);
            }

            EigenVector eigenvalues{};
            std::array<EigenVector, SourceDimCount> eigenvectors;

            math::computeEigenpairsViaPowerIteration(covCopy, eigenvectors, eigenvalues);

            // Store PCA basis and whitening scales
            for (std::size_t k{}; k < ReducedDimCount; ++k)
            {
                for (std::size_t j{}; j < SourceDimCount; ++j)
                    _pcaBasis[k][j] = static_cast<FloatType>(eigenvectors[k][j]);

                _pcaScale[k] = (eigenvalues[k] > 1e-15) ? static_cast<FloatType>(1.0 / std::sqrt(eigenvalues[k])) : FloatType{};
            }
        }

        _pcaReady = true;
        LMS_LOG(RECOMMENDATION, DEBUG, "Computing dataset stats DONE");
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::getReducedVector(const SourceVector& sourceVector, ReducedVector& reducedVector) const
    {
        SourceVector centeredSourceVector{ sourceVector };
        for (std::size_t i{}; i < SourceDimCount; ++i)
            centeredSourceVector[i] -= _sourceMeans[i];

        projectToReduced(centeredSourceVector, reducedVector);
        reducedVector.normalizeL2();
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::projectToReduced(const SourceVector& sourceVectorCentered, ReducedVector& reducedVector) const
    {
        assert(_pcaReady);
        math::projectOntoBasis(_pcaBasis, sourceVectorCentered, reducedVector, _pcaScale);
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeReducedFeatures()
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "ComputeReducedVectors");

        LMS_LOG(RECOMMENDATION, INFO, "Computing reduced vectors... Reducing from " << SourceDimCount << " to " << ReducedDimCount << " dimensions");

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        _trackVectors.clear();
        _vectors.clear();
        _vectors.reserve(_trackCount); // must keep pointers valid
        _releaseVectors.clear();
        _artistVectors.clear();

        Provider::visitVectors(session, [&](db::TrackId trackId, const SourceVector& sourceVector) {
            assert(_vectors.size() < _vectors.capacity());
            auto& reducedVector{ _vectors.emplace_back() };
            getReducedVector(sourceVector, reducedVector);
            _trackVectors.try_emplace(trackId, &reducedVector);
        });

        db::Release::find(session, db::Release::FindParameters{}, [&](const db::Release::pointer& release) {
            std::vector<std::reference_wrapper<const ReducedVector>> releaseTrackFeatures;

            db::Track::FindParameters params;
            params.setRelease(release->getId());

            const auto trackIds{ db::Track::findIds(session, params) };
            for (const db::TrackId trackId : trackIds.results)
            {
                const auto itFeatures{ _trackVectors.find(trackId) };
                if (itFeatures != std::cend(_trackVectors))
                {
                    assert(itFeatures->second);
                    releaseTrackFeatures.emplace_back(*itFeatures->second);
                }
            }

            if (!releaseTrackFeatures.empty())
                _releaseVectors.try_emplace(release->getId(), std::move(releaseTrackFeatures));
        });

        db::Artist::find(session, db::Artist::FindParameters{}, [&](const db::Artist::pointer& artist) {
            std::vector<std::reference_wrapper<const ReducedVector>> artistTrackVectors;

            {
                db::Track::FindParameters params;
                params.setArtist(artist->getId(), { db::TrackArtistLinkType::Artist });

                const auto trackIds{ db::Track::findIds(session, params) };
                for (const db::TrackId trackId : trackIds.results)
                {
                    const auto itFeatures{ _trackVectors.find(trackId) };
                    if (itFeatures != std::cend(_trackVectors))
                    {
                        assert(itFeatures->second);
                        artistTrackVectors.emplace_back(*itFeatures->second);
                    }
                }
            }

// TODO ALBUM ARTIST
#if 0
            {
                db::Release::FindParameters params;
                params.setArtist(artist->getId());

                const auto releaseIds{ db::Release::findIds(session, params) };
                for (const db::ReleaseId releaseId : releaseIds.results)
                {
                    const auto itReleaseFeatures{ _releaseFeatures.find(releaseId) };
                    if (itReleaseFeatures != std::cend(_releaseFeatures))
                    {
                        for (const auto* features : itReleaseFeatures->second)
                            medoidCalculator.add(features);
                    }
                }
            }
#endif

            if (!artistTrackVectors.empty())
                _artistVectors.try_emplace(artist->getId(), std::move(artistTrackVectors));
        });

        LMS_LOG(RECOMMENDATION, INFO, "Computed reduced vectors: " << _trackVectors.size() << " tracks, " << _releaseVectors.size() << " releases, " << _artistVectors.size() << " artists");
    }
} // namespace lms::recommendation
