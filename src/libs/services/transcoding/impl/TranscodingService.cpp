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

#include "TranscodingService.hpp"

#include "core/ILogger.hpp"

#include "TranscodingResourceHandler.hpp"

namespace lms::transcoding
{
    std::unique_ptr<ITranscodingService> createTranscodingService(core::IChildProcessManager& childProcessManager)
    {
        return std::make_unique<TranscodingService>(childProcessManager);
    }

    TranscodingService::TranscodingService(core::IChildProcessManager& childProcessManager)
        : _childProcessManager(childProcessManager)
    {
        LMS_LOG(TRANSCODING, INFO, "Service started!");
    }

    TranscodingService::~TranscodingService()
    {
        LMS_LOG(TRANSCODING, INFO, "Service stopped!");
    }

    std::unique_ptr<core::IResourceHandler> TranscodingService::createResourceHandler(const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength)
    {
        return std::make_unique<TranscodingResourceHandler>(inputParameters, outputParameters, estimateContentLength);
    }
} // namespace lms::transcoding
