/*
 * Copyright (C) 2025 Emeric Poupon
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
    class ScanStepArtistReconciliation : public ScanStepBase
    {
    public:
        using ScanStepBase::ScanStepBase;

    private:
        ScanStep getStep() const override { return ScanStep::ReconciliateArtists; }
        core::LiteralString getStepName() const override { return "Artist reconciliation"; }
        bool needProcess(const ScanContext& context) const override;
        void process(ScanContext& context) override;

        void updateLinksForArtistNameNoLongerMatch(ScanContext& context);
        void updateLinksWithArtistNameAmbiguity(ScanContext& context);
        void updateArtistInfoForArtistNameNoLongerMatch(ScanContext& context);
        void updateArtistInfoWithArtistNameAmbiguity(ScanContext& context);
    };
} // namespace lms::scanner
