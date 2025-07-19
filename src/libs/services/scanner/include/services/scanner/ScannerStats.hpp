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

#pragma once

#include <Wt/WDateTime.h>

#include <memory>
#include <vector>

#include "database/objects/TrackId.hpp"

#include "ScanErrors.hpp"

namespace lms::scanner
{
    enum class DuplicateReason
    {
        SameHash,
        SameTrackMBID,
    };

    struct ScanDuplicate
    {
        db::TrackId trackId;
        DuplicateReason reason;
    };

    // Alphabetical order
    enum class ScanStep
    {
        AssociateArtistImages,
        AssociateExternalLyrics,
        AssociatePlayListTracks,
        AssociateReleaseImages,
        AssociateTrackImages,
        CheckForDuplicatedFiles,
        CheckForRemovedFiles,
        ComputeClusterStats,
        Compact,
        FetchTrackFeatures,
        Optimize,
        ReconciliateArtists,
        ReloadSimilarityEngine,
        RemoveOrphanedDbEntries,
        ScanFiles,
        UpdateLibraryFields,
    };

    // reduced scan stats
    struct ScanStepStats
    {
        Wt::WDateTime startTime;

        std::size_t stepCount{};
        std::size_t stepIndex{};
        ScanStep currentStep;

        std::size_t totalElems{};
        std::size_t processedElems{};

        unsigned progress() const;
    };

    struct ScanStats
    {
        Wt::WDateTime startTime;
        Wt::WDateTime stopTime;

        std::size_t totalFileCount{}; // Total number of files (only valid after the file scan step)

        std::size_t skips{}; // no change since last scan
        std::size_t scans{}; // count of scanned files

        std::size_t additions{}; // added in DB
        std::size_t deletions{}; // removed from DB
        std::size_t updates{};   // updated file in DB
        std::size_t failures{};  // scan failure

        std::size_t featuresFetched{}; // features fetched in DB

        static constexpr std::size_t maxStoredErrorCount{ 5'000 }; // TODO make this configurable
        std::vector<std::shared_ptr<ScanError>> errors;
        std::size_t errorsCount{}; // maybe bigger than errors.size() if too many errors
        std::vector<ScanDuplicate> duplicates;

        std::size_t getTotalFileCount() const;
        std::size_t getChangesCount() const;
    };
} // namespace lms::scanner
