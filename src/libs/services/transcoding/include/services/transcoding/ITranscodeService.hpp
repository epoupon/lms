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

#include <memory>

#include "audio/TranscodeTypes.hpp"

namespace lms
{
    namespace core
    {
        class IChildProcessManager;
        class IResourceHandler;
    } // namespace core

    namespace db
    {
        class IDb;
    }
} // namespace lms

namespace lms::transcoding
{
    class ITranscodeService
    {
    public:
        virtual ~ITranscodeService() = default;

        // virtual std::unique_ptr<IAudioFileInfo> parseAudioFileInfo(const std::filesystem::path& p) const = 0;

        virtual std::unique_ptr<core::IResourceHandler> createTranscodeResourceHandler(const audio::TranscodeParameters& parameters, bool estimateContentLength = false) = 0;
    };

    std::unique_ptr<ITranscodeService> createTranscodeService(db::IDb& db, core::IChildProcessManager& childProcessManager);
} // namespace lms::transcoding
