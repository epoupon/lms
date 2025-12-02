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

#include <chrono>
#include <memory>

#include "core/UUID.hpp"

#include "payloads/StreamDetails.hpp"

#include "AudioFileId.hpp"

namespace lms::api::subsonic
{
    // Keeps track of decisions using UUIDs
    class ITranscodeDecisionTracker
    {
    public:
        virtual ~ITranscodeDecisionTracker() = default;

        using Clock = std::chrono::steady_clock;

        struct Entry
        {
            Clock::time_point addedTimePoint;
            AudioFileId audioFileId;
            StreamDetails targetStreamInfo;
        };

        virtual core::UUID add(AudioFileId audioFileId, const StreamDetails& targetStreamInfo) = 0;
        virtual std::shared_ptr<Entry> get(const core::UUID& uuid) = 0;
    };

    ITranscodeDecisionTracker& getTranscodeDecisionTracker();
} // namespace lms::api::subsonic