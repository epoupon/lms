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

#include <algorithm>
#include <thread>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"

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

        std::size_t getThreadCount()
        {
            const unsigned long configThreadCount{ core::Service<core::IConfig>::get()->getULong("transcode-thread-count", 0) };
            return configThreadCount ? configThreadCount : std::max<unsigned long>(1, std::thread::hardware_concurrency());
        }
    } // namespace

    std::unique_ptr<ITranscodeService> createTranscodeService()
    {
        return std::make_unique<TranscodeService>();
    }

    TranscodeService::TranscodeService()
        : _ioContextRunner{ _ioContext, getThreadCount(), "Transcoding" }
    {
        LMS_LOG(TRANSCODING, INFO, "Service started!");
    }

    TranscodeService::~TranscodeService()
    {
        LMS_LOG(TRANSCODING, INFO, "Service stopped!");
    }

    std::shared_ptr<core::IResourceHandler> TranscodeService::createTranscodeResourceHandler(const audio::TranscodeParameters& parameters, bool estimateContentLength)
    {
        std::optional<std::size_t> estimatedContentLength;

        if (estimateContentLength)
        {
            if (!parameters.outputParameters.bitrate)
                LMS_LOG(TRANSCODING, WARNING, "No output bitrate set: not estimating content length");
            else if (parameters.inputParameters.offset >= parameters.inputParameters.audioProperties.duration)
                LMS_LOG(TRANSCODING, WARNING, "Offset " << parameters.inputParameters.offset << " is greater than audio file duration " << parameters.inputParameters.audioProperties.duration << ": not estimating content length");
            else
                estimatedContentLength = doEstimateContentLength(*parameters.outputParameters.bitrate, parameters.inputParameters.audioProperties.duration - parameters.inputParameters.offset);
        }

        return std::make_shared<transcoding::ResourceHandler>(_ioContext, parameters, estimatedContentLength);
    }
} // namespace lms::transcoding
