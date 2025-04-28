/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ScanStepBase.hpp"

namespace lms::scanner
{
    class ScanStepRemoveOrphanedDbEntries : public ScanStepBase
    {
    public:
        using ScanStepBase::ScanStepBase;

    private:
        core::LiteralString getStepName() const override { return "Remove orphaned DB entries"; }
        ScanStep getStep() const override { return ScanStep::RemoveOrphanedDbEntries; }
        bool needProcess(const ScanContext& context) const override;
        void process(ScanContext& context) override;

        void removeOrphanedClusters(ScanContext& context);
        void removeOrphanedClusterTypes(ScanContext& context);
        void removeOrphanedArtists(ScanContext& context);
        void removeOrphanedReleases(ScanContext& context);
        void removeOrphanedReleaseTypes(ScanContext& context);
        void removeOrphanedLabels(ScanContext& context);
        void removeOrphanedCountries(ScanContext& context);
        void removeOrphanedDirectories(ScanContext& context);
        void removeOrphanedTrackEmbeddedImages(ScanContext& context);

        template<typename T>
        void removeOrphanedEntries(ScanContext& context);
    };
} // namespace lms::scanner
