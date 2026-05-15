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
#include <unordered_map>
#include <utility>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "audio/MusicNNEmbeddings.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "math/CosineDistance.hpp"
#include "math/CovarianceCalculator.hpp"
#include "math/MedoidCalculator.hpp"
#include "math/PrincipalComponents.hpp"
#include "math/StatsAccumulator.hpp"

#include "Types.hpp"

namespace lms::recommendation
{
    namespace detail
    {
        template<typename Vector>
        FloatType chamferDistanceAtoB(
            const std::span<const Vector* const>& A,
            const std::span<const Vector* const>& B)
        {
            assert(!A.empty());
            assert(!B.empty());

            FloatType total{};

            for (const auto& a : A)
            {
                typename Vector::value_type bestDist{ std::numeric_limits<FloatType>::max() };

                const math::CosineDistance distFunc{ *a };

                for (const auto& b : B)
                {
                    const FloatType dist{ distFunc(*b) };
                    if (dist < bestDist)
                        bestDist = dist;
                }

                total += bestDist;
            }

            return total / static_cast<FloatType>(A.size());
        }
    } // namespace detail

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    AudioSimilarityEngine<Provider, ReducedDimCount>::AudioSimilarityEngine(db::IDb& db)
        : _db{ db }
        , _ioContextRunner{ _ioContext, 1, "FeaturesEngine" }
    {
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
        const math::CosineDistance distFunc{ queryVector };

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

        for (const auto& [candidateId, candidateReleaseVectors] : _releaseVectors)
        {
            if (candidateId == releaseId || candidateReleaseVectors.empty())
                continue;

            const FloatType lhsToRhs{ detail::chamferDistanceAtoB(
                std::span<const ReducedVector* const>{ queryReleaseFeatures },
                std::span<const ReducedVector* const>{ candidateReleaseVectors }) };
            const FloatType rhsToLhs{ detail::chamferDistanceAtoB(
                std::span<const ReducedVector* const>{ candidateReleaseVectors },
                std::span<const ReducedVector* const>{ queryReleaseFeatures }) };
            const FloatType distance{ (lhsToRhs + rhsToLhs) * FloatType{ 0.5F } };

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

        for (const auto& [candidateId, candidateArtistFeatures] : _artistVectors)
        {
            if (candidateId == artistId || candidateArtistFeatures.empty())
                continue;

            const FloatType lhsToRhs{ detail::chamferDistanceAtoB(
                std::span<const ReducedVector* const>{ queryArtistFeatures },
                std::span<const ReducedVector* const>{ candidateArtistFeatures }) };
            const FloatType rhsToLhs{ detail::chamferDistanceAtoB(
                std::span<const ReducedVector* const>{ candidateArtistFeatures },
                std::span<const ReducedVector* const>{ queryArtistFeatures }) };
            const FloatType distance{ (lhsToRhs + rhsToLhs) * FloatType{ 0.5F } };

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

        // computeReleaseHitRank();
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
        reducedVector.normalizeL2(); // TODO further test with/without L2 normalization
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
            std::vector<const ReducedVector*> releaseTrackFeatures;

            db::Track::FindParameters params;
            params.setRelease(release->getId());

            const auto trackIds{ db::Track::findIds(session, params) };
            for (const db::TrackId trackId : trackIds.results)
            {
                const auto itFeatures{ _trackVectors.find(trackId) };
                if (itFeatures != std::cend(_trackVectors))
                {
                    assert(itFeatures->second);
                    releaseTrackFeatures.push_back(itFeatures->second);
                }
            }

            if (!releaseTrackFeatures.empty())
                _releaseVectors.try_emplace(release->getId(), std::move(releaseTrackFeatures));
        });

        db::Artist::find(session, db::Artist::FindParameters{}, [&](const db::Artist::pointer& artist) {
            std::vector<const ReducedVector*> artistTrackVectors;

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
                        artistTrackVectors.push_back(itFeatures->second);
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

    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    void AudioSimilarityEngine<Provider, ReducedDimCount>::computeReleaseHitRank()
    {
        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        constexpr std::size_t maxSimilarTrackCount{ 250 };

        LMS_LOG(RECOMMENDATION, DEBUG, "Getting all features");

        struct TrackDesc
        {
            db::ReleaseId releaseId;
            const ReducedVector* vectors{};
        };
        std::unordered_map<db::TrackId, TrackDesc> trackDescs;

        for (const auto& [trackId, vectors] : _trackVectors)
        {
            assert(vectors);

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

            trackDescs[trackId] = TrackDesc{ .releaseId = releaseId, .vectors = vectors };
            assert(trackDescs[trackId].vectors);
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

                    math::CosineDistance dist{ *trackDesc.vectors };
                    std::sort(sortedTrackIds.begin(), sortedTrackIds.end(), [&](db::TrackId trackA, db::TrackId trackB) {
                        return dist(*trackDescs[trackA].vectors) < dist(*trackDescs[trackB].vectors);
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
