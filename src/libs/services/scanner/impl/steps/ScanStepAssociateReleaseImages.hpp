/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <string>
#include <vector>

#include "ScanStepBase.hpp"

namespace lms::scanner
{
    class ScanStepAssociateReleaseImages : public ScanStepBase
    {
    public:
        ScanStepAssociateReleaseImages(InitParams& initParams);
        ~ScanStepAssociateReleaseImages() override = default;
        ScanStepAssociateReleaseImages(const ScanStepAssociateReleaseImages&) = delete;
        ScanStepAssociateReleaseImages& operator=(const ScanStepAssociateReleaseImages&) = delete;

    private:
        ScanStep getStep() const override { return ScanStep::AssociateReleaseImages; }
        core::LiteralString getStepName() const override { return "Associate release images"; }
        bool needProcess(const ScanContext& context) const override;
        void process(ScanContext& context) override;

        const std::vector<std::string> _releaseImageFileNames;
    };
} // namespace lms::scanner
