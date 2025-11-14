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

#include "TranscodeService.hpp"

#include "audio/ITranscoder.hpp"
#include "core/ILogger.hpp"
#include "core/UUID.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"

#include "TranscodeResourceHandler.hpp"

namespace lms::transcoding
{
    namespace
    {
        std::size_t doEstimateContentLength(std::size_t bitrate, std::chrono::milliseconds duration)
        {
            const std::size_t estimatedContentLength{ static_cast<size_t>((bitrate / 8 * duration.count()) / 1000) };
            return estimatedContentLength;
        }
    } // namespace

    std::unique_ptr<ITranscodeService> createTranscodeService(db::IDb& db, core::IChildProcessManager& childProcessManager)
    {
        return std::make_unique<TranscodeService>(db, childProcessManager);
    }

    TranscodeService::TranscodeService(db::IDb& db, core::IChildProcessManager& childProcessManager)
        : _db{ db }
        , _childProcessManager(childProcessManager)
    {
        LMS_LOG(TRANSCODING, INFO, "Service started!");
    }

    TranscodeService::~TranscodeService()
    {
        LMS_LOG(TRANSCODING, INFO, "Service stopped!");
    }

    std::unique_ptr<core::IResourceHandler> TranscodeService::createTranscodeResourceHandler(const audio::TranscodeParameters& parameters, bool estimateContentLength)
    {
        std::optional<std::size_t> estimatedContentLength;

        if (estimateContentLength)
        {
            if (parameters.inputParameters.offset < parameters.inputParameters.duration)
                estimatedContentLength = doEstimateContentLength(*parameters.outputParameters.bitrate, parameters.inputParameters.duration - parameters.inputParameters.offset);
            else
                LMS_LOG(TRANSCODING, WARNING, "Offset " << parameters.inputParameters.offset << " is greater than audio file duration " << parameters.inputParameters.duration << ": not estimating content length");
        }

        return std::make_unique<transcoding::ResourceHandler>(parameters, estimatedContentLength);
    }
} // namespace lms::transcoding
