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

#include "FileScanOperationBase.hpp"

namespace lms::metadata
{
    class IAudioFileParser;
} // namespace lms::metadata

namespace lms::scanner
{
    namespace cue
    {
        struct ParsedCueTrack;
    }

    class CueFileScanOperation : public FileScanOperationBase
    {
    public:
        CueFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, metadata::IAudioFileParser& parser);
        ~CueFileScanOperation() override;

        CueFileScanOperation(const CueFileScanOperation&) = delete;
        CueFileScanOperation& operator=(const CueFileScanOperation&) = delete;

    private:
        core::LiteralString getName() const override { return "ScanCueFile"; }
        void scan() override;
        OperationResult processResult() override;

        metadata::IAudioFileParser& _parser;
        std::vector<cue::ParsedCueTrack> _parsedFile;
    };
} // namespace lms::scanner
