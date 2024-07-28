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

#include "database/DirectoryId.hpp"

#include "ScanStepBase.hpp"

namespace lms::scanner
{
    class ScanStepUpdateLibraryFields : public ScanStepBase
    {
    public:
        using ScanStepBase::ScanStepBase;

    private:
        core::LiteralString getStepName() const override { return "Update Library fields"; }
        ScanStep getStep() const override { return ScanStep::UpdateLibraryFields; }
        void process(ScanContext& context) override;

        void processDirectories(ScanContext& context);
        void processDirectory(ScanContext& context, const ScannerSettings::MediaLibraryInfo& mediaLibrary);
    };
} // namespace lms::scanner
