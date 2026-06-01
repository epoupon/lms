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
#include <memory>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Random.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "math/ChamferDistance.hpp"
#include "math/CovarianceCalculator.hpp"
#include "math/MedoidCalculator.hpp"
#include "math/NormalizedCosineDistance.hpp"
#include "math/PrincipalComponents.hpp"
#include "math/StatsAccumulator.hpp"

#include "track-selection-constraints/DuplicateTrackConstraint.hpp"
#include "track-selection-constraints/InterpolationFitConstraint.hpp"
#include "track-selection-constraints/MaxDistanceConstraint.hpp"
#include "track-selection-constraints/SameArtistConstraint.hpp"
#include "track-selection-constraints/SameReleaseConstraint.hpp"
#include "track-selection-constraints/SmoothTransitionConstraint.hpp"

#include "Types.hpp"

#define LOG(sev, message) LMS_LOG(RECOMMENDATION, sev, "[audio-similarity] " << message)

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

                neighbors.push_back({ .id = trackId, .distance = distFunc(*trackVector) });
            }

            maxNeighbors = std::min(maxNeighbors, neighbors.size());
            if (maxNeighbors == 0)
                return {};

            std::nth_element(neighbors.begin(), neighbors.begin() + static_cast<std::ptrdiff_t>(maxNeighbors), neighbors.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.distance < rhs.distance;
            });
            neighbors.resize(maxNeighbors);
            std::sort(neighbors.begin(), neighbors.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.distance < rhs.distance;
            });
            return neighbors;
        }
    } // namespace detail

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    AudioSimilarityEngine<Provider, ReducedDimCount>::AudioSimilarityEngine(db::IDb& db)
        : _db{ db }
    {
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    AudioSimilarityEngine<Provider, ReducedDimCount>::~AudioSimilarityEngine() = default;

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::initializeConstraints()
    {
        constexpr float interpolationFitWeight{ 0.8F };
        constexpr float smoothTransitionWeight{ 0.2F };
        constexpr float sameReleaseWeight{ 0.5F };
        constexpr float sameArtistWeight{ 0.5F };

        _similarityEvaluator = {};
        _similarityEvaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
        _similarityEvaluator.addHardConstraint(std::make_unique<MaxDistanceConstraint>(_trackDistanceThreshold));
        _similarityEvaluator.addSoftConstraint(std::make_unique<InterpolationFitConstraint>(), interpolationFitWeight);
        _similarityEvaluator.addSoftConstraint(std::make_unique<SmoothTransitionConstraint>(), smoothTransitionWeight);
        _similarityEvaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(_trackMetadata), sameReleaseWeight);
        _similarityEvaluator.addSoftConstraint(std::make_unique<SameArtistConstraint>(_trackMetadata), sameArtistWeight);

        _pathEvaluator = {};
        _pathEvaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
        _pathEvaluator.addSoftConstraint(std::make_unique<InterpolationFitConstraint>(), interpolationFitWeight);
        _pathEvaluator.addSoftConstraint(std::make_unique<SmoothTransitionConstraint>(), smoothTransitionWeight);
        _pathEvaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(_trackMetadata), sameReleaseWeight);
        _pathEvaluator.addSoftConstraint(std::make_unique<SameArtistConstraint>(_trackMetadata), sameArtistWeight);
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>

    TrackResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "Find similar tracks from tracklist");

        if (maxCount == 0)
            return {};

        std::vector<db::TrackId> trackIds;
        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            const db::TrackList::pointer trackList{ db::TrackList::find(session, tracklistId) };
            if (!trackList)
                return {};

            trackIds = trackList->getTrackIds();
        }

        if (trackIds.empty())
            return {};

        return findSimilarTracks(trackIds, maxCount);
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>

    TrackResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarTracks(std::span<const db::TrackId> tracksId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "Find similar tracks");

        TrackResults res;
        if (maxCount == 0 || tracksId.empty())
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

        // Oversample to give the diversity selection enough candidates to work with
        static constexpr std::size_t oversamplingFactor{ 5 };
        const std::size_t candidateCount{ std::min(maxCount * oversamplingFactor, rankedTracks.size()) };
        std::partial_sort(std::begin(rankedTracks), std::next(std::begin(rankedTracks), static_cast<std::ptrdiff_t>(candidateCount)), std::end(rankedTracks), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });
        rankedTracks.resize(candidateCount);

        // Greedy selection: at each step pick the candidate with the lowest penalized score.
        // distanceToPrevious is the cosine distance to the last selected track, so that
        // SmoothTransitionConstraint penalises large acoustic jumps between consecutive results.
        // Pre-seed selectedTracks with the input tracks so that soft constraints (same release,
        // same artist) treat them as already taken, preventing the first results from being
        // from the same release/artist as the inputs.
        std::vector<db::TrackId> selectedTracks(std::cbegin(tracksId), std::cend(tracksId));
        selectedTracks.reserve(selectedTracks.size() + maxCount);
        res.reserve(maxCount);

        const ReducedVector* previousVector{};

        while (res.size() < maxCount && !rankedTracks.empty())
        {
            std::optional<std::size_t> bestIdx;
            float bestScore{ std::numeric_limits<float>::max() };

            for (std::size_t i{}; i < rankedTracks.size(); ++i)
            {
                const auto& [candidateId, distanceToQuery]{ rankedTracks[i] };
                const ReducedVector* candidateVector{ _trackVectors.at(candidateId) };
                const float distanceToPrevious{ previousVector ? math::NormalizedCosineDistance{ *previousVector }(*candidateVector) : 0.F };

                const TrackCandidateContext context{
                    .candidateTrackId = candidateId,
                    .selectedTracks = selectedTracks,
                    .distanceToQuery = distanceToQuery,
                    .distanceToPrevious = distanceToPrevious,
                };

                if (_similarityEvaluator.rejects(context))
                    continue;

                const float score{ _similarityEvaluator.score(context) };
                if (score < bestScore)
                {
                    bestScore = score;
                    bestIdx = i;
                }
            }

            if (!bestIdx)
                break;

            const auto& [selectedId, distanceToQuery]{ rankedTracks[*bestIdx] };
            res.push_back({ .id = selectedId, .distance = distanceToQuery });
            selectedTracks.push_back(selectedId);
            previousVector = _trackVectors.at(selectedId);
            rankedTracks.erase(std::begin(rankedTracks) + static_cast<std::ptrdiff_t>(*bestIdx));
        }

        return res;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    TrackResults AudioSimilarityEngine<Provider, ReducedDimCount>::findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "Find track similarity path");

        if (maxCount == 0)
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

        auto evaluateCandidates = [&](const TrackResults& neighborList) -> std::optional<db::TrackId> {
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

                if (_pathEvaluator.rejects(context))
                    continue;

                const float score{ _pathEvaluator.score(context) };
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

        const math::NormalizedCosineDistance startDistFunc{ startVector };
        for (const db::TrackId trackId : path)
        {
            const auto* trackVector{ _trackVectors.at(trackId) };
            results.push_back({ .id = trackId, .distance = startDistFunc(*trackVector) });
        }

        return results;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    ReleaseResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarReleases(
        db::ReleaseId releaseId,
        std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "Find similar releases");

        ResultContainer<db::ReleaseId> res;
        if (maxCount == 0)
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

            if (distance <= _releaseDistanceThreshold)
                rankedReleases.emplace_back(candidateId, distance);
        }

        const std::size_t resultCount{ std::min(maxCount, rankedReleases.size()) };
        std::partial_sort(std::begin(rankedReleases), std::next(std::begin(rankedReleases), resultCount), std::end(rankedReleases), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back({ .id = rankedReleases[i].first, .distance = rankedReleases[i].second });

        return res;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    ArtistResults AudioSimilarityEngine<Provider, ReducedDimCount>::findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "Find similar artists");

        ArtistResults res;
        if (maxCount == 0)
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

            if (distance <= _artistDistanceThreshold)
                rankedArtists.emplace_back(candidateId, distance);
        }

        const std::size_t resultCount{ std::min(maxCount, rankedArtists.size()) };
        std::partial_sort(std::begin(rankedArtists), std::next(std::begin(rankedArtists), resultCount), std::end(rankedArtists), [](const auto& lhs, const auto& rhs) {
            return lhs.second < rhs.second;
        });

        res.reserve(resultCount);
        for (std::size_t i{}; i < resultCount; ++i)
            res.push_back({ .id = rankedArtists[i].first, .distance = rankedArtists[i].second });

        return res;
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::load()
    {
        LMS_SCOPED_TRACE_OVERVIEW("AudioSimilarityEngine", "Loading");

        LOG(INFO, "loading...");

        computeDatasetStats();
        computeReducedFeatures();
        computeTrackDistanceThreshold();
        computeReleaseDistanceThreshold();
        computeArtistDistanceThreshold();
        initializeConstraints();

        LOG(INFO, "loading complete!");
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeDatasetStats()
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "Compute dataset stats");

        LOG(DEBUG, "computing dataset stats...");

        _pcaReady = false;
        _trackCount = 0;

        std::array<math::StatsAccumulator<FloatType>, SourceDimCount> statsAccumulators;

        {
            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            Provider::visitVectors(session, [&]([[maybe_unused]] db::TrackId trackId, const SourceVector& sourceVector) {
                for (std::size_t i{}; i < SourceDimCount; ++i)
                    statsAccumulators[i].add(sourceVector[i]);

                _trackCount++;
            });
        }

        for (std::size_t featureIndex{}; featureIndex < SourceDimCount; ++featureIndex)
            _sourceMeans[featureIndex] = static_cast<FloatType>(statsAccumulators[featureIndex].getMean());

        // Compute covariance
        using AudioFeatureMatrix = math::SquareMatrix<FloatType, SourceDimCount>;
        const auto covariance{ std::make_unique<AudioFeatureMatrix>() };
        {
            const auto calculator{ std::make_unique<math::CovarianceMatrixCalculator<SourceDimCount, FloatType>>() };

            db::Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            Provider::visitVectors(session, [&](db::TrackId, SourceVector& sourceVector) {
                for (std::size_t i{}; i < SourceDimCount; ++i)
                    sourceVector[i] -= _sourceMeans[i];

                calculator->add(sourceVector);
            });

            calculator->finalizeSample(*covariance);
        }

        // PCA via power iteration + deflation in double precision
        {
            using EigenMatrix = math::SquareMatrix<double, SourceDimCount>;
            using EigenVector = math::Vector<SourceDimCount, double>;

            auto covarianceCopy{ std::make_unique<EigenMatrix>() };
            for (std::size_t i{}; i < SourceDimCount; ++i)
            {
                for (std::size_t j{}; j < SourceDimCount; ++j)
                    (*covarianceCopy)[i][j] = static_cast<double>((*covariance)[i][j]);
            }

            EigenVector eigenValues{};
            auto eigenVectors{ std::make_unique<std::array<EigenVector, SourceDimCount>>() };

            math::computeEigenpairsViaPowerIteration(*covarianceCopy, *eigenVectors, eigenValues);

            // Store PCA basis and whitening scales
            for (std::size_t k{}; k < ReducedDimCount; ++k)
            {
                for (std::size_t j{}; j < SourceDimCount; ++j)
                    _pcaBasis[k][j] = static_cast<FloatType>((*eigenVectors)[k][j]);

                _pcaScale[k] = (eigenValues[k] > 1e-15) ? static_cast<FloatType>(1.0 / std::sqrt(eigenValues[k])) : FloatType{};
            }
        }

        _pcaReady = true;
        LOG(DEBUG, "computing dataset stats done");
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

        LOG(INFO, "computing reduced vectors... Reducing from " << SourceDimCount << " to " << ReducedDimCount << " dimensions");

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        _trackVectors.clear();
        _vectors.clear();
        _vectors.reserve(_trackCount); // must keep pointers valid
        _releaseVectors.clear();
        _artistVectors.clear();
        _trackMetadata.clear();

        Provider::visitVectors(session, [&](db::TrackId trackId, const SourceVector& sourceVector) {
            if (_vectors.size() >= _trackCount)
                return; // more tracks appeared since computeDatasetStats(); skip to avoid reallocation (a further reload will include them)
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
                    _trackMetadata[trackId].releaseId = release->getId();
                }
            }

            if (!releaseTrackFeatures.empty())
                _releaseVectors.try_emplace(release->getId(), std::move(releaseTrackFeatures));
        });

        db::Artist::find(session, db::Artist::FindParameters{}, [&](const db::Artist::pointer& artist) {
            std::unordered_set<db::TrackId> artistTrackIds;

            // Track-level artists
            {
                db::Track::FindParameters params;
                params.setArtist(artist->getId(), { db::TrackArtistLinkType::Artist });
                for (const db::TrackId trackId : db::Track::findIds(session, params).results)
                    artistTrackIds.insert(trackId);
            }

            // Album-level artists
            {
                db::Release::FindParameters params;
                params.setArtist(artist->getId());
                for (const db::ReleaseId releaseId : db::Release::findIds(session, params).results)
                {
                    if (_releaseVectors.contains(releaseId))
                    {
                        db::Track::FindParameters trackParams;
                        trackParams.setRelease(releaseId);
                        for (const db::TrackId trackId : db::Track::findIds(session, trackParams).results)
                            artistTrackIds.insert(trackId);
                    }
                }
            }

            // Build vectors from deduplicated track IDs
            std::vector<std::reference_wrapper<const ReducedVector>> artistTrackVectors;
            artistTrackVectors.reserve(artistTrackIds.size());
            for (const db::TrackId trackId : artistTrackIds)
            {
                const auto it{ _trackVectors.find(trackId) };
                if (it != std::cend(_trackVectors))
                {
                    assert(it->second);
                    artistTrackVectors.emplace_back(*it->second);
                    _trackMetadata[trackId].artistIds.push_back(artist->getId());
                }
            }

            if (!artistTrackVectors.empty())
                _artistVectors.try_emplace(artist->getId(), std::move(artistTrackVectors));
        });

        // Sort artistIds in each TrackMetadata entry for set-intersection in SameArtistConstraint
        for (auto& [trackId, metadata] : _trackMetadata)
            std::sort(metadata.artistIds.begin(), metadata.artistIds.end());

        LOG(INFO, "computed reduced vectors: " << _trackVectors.size() << " tracks, " << _releaseVectors.size() << " releases, " << _artistVectors.size() << " artists");
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeTrackDistanceThreshold()
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "computeTrackDistanceThreshold");

        constexpr std::size_t maxSampleCount{ 500 };
        constexpr float stdDevMultiplier{ 2.F };

        const std::size_t sampleCount{ std::min(_trackVectors.size(), maxSampleCount) };

        LOG(INFO, "computing track distance threshold using " << sampleCount << " samples...");

        // Collect all vector pointers and shuffle for an unbiased random sample.
        std::vector<const ReducedVector*> allVectors;
        allVectors.reserve(_trackVectors.size());
        for (const auto& [id, vec] : _trackVectors)
            allVectors.push_back(vec);

        std::minstd_rand randomEngine{ 42 };
        core::random::shuffleContainer(randomEngine, allVectors);

        math::StatsAccumulator<FloatType> stats;
        for (std::size_t i{}; i < sampleCount; ++i)
        {
            const ReducedVector* queryVector{ allVectors[i] };
            const math::NormalizedCosineDistance distFunc{ *queryVector };
            FloatType minDist{ std::numeric_limits<FloatType>::max() };

            for (const ReducedVector* candidateVector : allVectors)
            {
                if (candidateVector == queryVector)
                    continue;

                const FloatType d{ distFunc(*candidateVector) };
                if (d < minDist)
                    minDist = d;
            }

            stats.add(minDist);
        }

        if (stats.getCount() >= 2)
            _trackDistanceThreshold = stats.getMean() + stdDevMultiplier * stats.getSampleStdDev();
        else
            _trackDistanceThreshold = std::numeric_limits<FloatType>::max();

        LOG(INFO, "track distance threshold = " << _trackDistanceThreshold);
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeReleaseDistanceThreshold()
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "ComputeReleaseDistanceThreshold");

        constexpr std::size_t maxSampleCount{ 200 };
        constexpr float stdDevMultiplier{ 2.F };
        using CosineDistance = math::NormalizedCosineDistance<ReducedVector::getSize(), FloatType>;

        std::vector<const std::vector<std::reference_wrapper<const ReducedVector>>*> allProfiles;
        allProfiles.reserve(_releaseVectors.size());
        for (const auto& [id, vecs] : _releaseVectors)
            allProfiles.push_back(&vecs);

        const std::size_t sampleCount{ std::min(allProfiles.size(), maxSampleCount) };
        LOG(INFO, "computing release distance threshold using " << sampleCount << " samples...");

        std::minstd_rand randomEngine{ 42 };
        core::random::shuffleContainer(randomEngine, allProfiles);

        math::StatsAccumulator<FloatType> stats;
        for (std::size_t i{}; i < sampleCount; ++i)
        {
            FloatType minDist{ std::numeric_limits<FloatType>::max() };
            for (const auto* candidate : allProfiles)
            {
                if (candidate == allProfiles[i])
                    continue;

                const FloatType d{ math::symmetricalChamferDistance<CosineDistance>(*allProfiles[i], *candidate) };
                if (d < minDist)
                    minDist = d;
            }
            if (minDist < std::numeric_limits<FloatType>::max())
                stats.add(minDist);
        }

        if (stats.getCount() >= 2)
            _releaseDistanceThreshold = stats.getMean() + stdDevMultiplier * stats.getSampleStdDev();
        else
            _releaseDistanceThreshold = std::numeric_limits<FloatType>::max();

        LOG(INFO, "release distance threshold = " << _releaseDistanceThreshold);
    }

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeArtistDistanceThreshold()
    {
        LMS_SCOPED_TRACE_DETAILED("AudioSimilarityEngine", "ComputeArtistDistanceThreshold");

        constexpr std::size_t maxSampleCount{ 200 };
        constexpr float stdDevMultiplier{ 2.F };
        using CosineDistance = math::NormalizedCosineDistance<ReducedVector::getSize(), FloatType>;

        std::vector<const std::vector<std::reference_wrapper<const ReducedVector>>*> allProfiles;
        allProfiles.reserve(_artistVectors.size());
        for (const auto& [id, vecs] : _artistVectors)
            allProfiles.push_back(&vecs);

        const std::size_t sampleCount{ std::min(allProfiles.size(), maxSampleCount) };
        LOG(INFO, "computing artist distance threshold using " << sampleCount << " samples...");

        std::minstd_rand randomEngine{ 42 };
        core::random::shuffleContainer(randomEngine, allProfiles);

        math::StatsAccumulator<FloatType> stats;
        for (std::size_t i{}; i < sampleCount; ++i)
        {
            FloatType minDist{ std::numeric_limits<FloatType>::max() };
            for (const auto* candidate : allProfiles)
            {
                if (candidate == allProfiles[i])
                    continue;

                const FloatType d{ math::symmetricalChamferDistance<CosineDistance>(*allProfiles[i], *candidate) };
                if (d < minDist)
                    minDist = d;
            }
            if (minDist < std::numeric_limits<FloatType>::max())
                stats.add(minDist);
        }

        if (stats.getCount() >= 2)
            _artistDistanceThreshold = stats.getMean() + stdDevMultiplier * stats.getSampleStdDev();
        else
            _artistDistanceThreshold = std::numeric_limits<FloatType>::max();

        LOG(INFO, "artist distance threshold = " << _artistDistanceThreshold);
    }
} // namespace lms::recommendation

#undef LOG
