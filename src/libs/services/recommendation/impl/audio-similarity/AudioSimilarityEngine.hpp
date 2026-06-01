/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <functional>
#include <unordered_map>
#include <vector>

#include "database/Object.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "math/Vector.hpp"

#include "AudioVectorProvider.hpp"
#include "IEngine.hpp"
#include "Types.hpp"
#include "track-selection-constraints/TrackCandidateEvaluator.hpp"
#include "track-selection-constraints/TrackMetadata.hpp"

namespace lms::recommendation
{
    template<AudioVectorProvider Provider, std::size_t ReducedDimCount>
    class AudioSimilarityEngine : public IEngine
    {
    public:
        AudioSimilarityEngine(db::IDb& db);
        ~AudioSimilarityEngine() override;

        AudioSimilarityEngine(const AudioSimilarityEngine&) = delete;
        AudioSimilarityEngine& operator=(const AudioSimilarityEngine&) = delete;

    private:
        using SourceVector = typename Provider::Vector;
        using ReducedVector = math::Vector<ReducedDimCount, FloatType>;
        static inline constexpr std::size_t SourceDimCount{ SourceVector::getSize() };

        void load() override;

        TrackResults findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackResults findSimilarTracks(std::span<const db::TrackId> tracksId, std::size_t maxCount) const override;
        TrackResults findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const override;
        ReleaseResults findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistResults findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

        void initializeConstraints();

        void computeDatasetStats();
        void computeReducedFeatures();
        void computeTrackDistanceThreshold();
        void computeReleaseDistanceThreshold();
        void computeArtistDistanceThreshold();

        void getReducedVector(const SourceVector& sourceVector, ReducedVector& output) const;
        void projectToReduced(const SourceVector& sourceVectorCentered, ReducedVector& output) const;

        db::IDb& _db;

        // Stats, used to normalize input data
        std::size_t _trackCount{};
        SourceVector _sourceMeans;

        // PCA basis: top pcaDimCount eigenvectors (rows) and whitening scales
        std::array<std::array<FloatType, SourceDimCount>, ReducedDimCount> _pcaBasis{};
        std::array<FloatType, ReducedDimCount> _pcaScale{};
        bool _pcaReady{};

        // In-memory cache of reduced feature vectors
        std::vector<ReducedVector> _vectors;
        std::unordered_map<db::TrackId, const ReducedVector*> _trackVectors;
        std::unordered_map<db::ReleaseId, std::vector<std::reference_wrapper<const ReducedVector>>> _releaseVectors;
        std::unordered_map<db::ArtistId, std::vector<std::reference_wrapper<const ReducedVector>>> _artistVectors;
        TrackMetadataMap _trackMetadata;

        FloatType _trackDistanceThreshold{};
        FloatType _releaseDistanceThreshold{};
        FloatType _artistDistanceThreshold{};
        TrackCandidateEvaluator _similarityEvaluator;
        TrackCandidateEvaluator _pathEvaluator;
    };
} // namespace lms::recommendation
