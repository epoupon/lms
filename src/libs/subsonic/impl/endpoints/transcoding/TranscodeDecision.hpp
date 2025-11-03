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

#include <vector>

#include "responses/StreamDetails.hpp"

namespace lms::audio
{
    struct AudioProperties;
}

namespace lms::api::subsonic
{
    class ClientInfo;

    namespace details
    {
        enum class TranscodeReason
        {
            AudioCodecNotSupported,
            AudioBitrateNotSupported,
            AudioChannelsNotSupported,
            AudioSampleRateNotSupported,
            AudioBitdepthNotSupported,
            ContainerNotSupported,
            ProtocolNotSupported
        };

        struct DirectPlayResult
        {
            bool operator==(const DirectPlayResult&) const = default;
        };

        struct TranscodeResult
        {
            std::vector<TranscodeReason> reasons;
            StreamDetails targetStreamInfo;

            bool operator==(const TranscodeResult&) const = default;
        };

        struct FailureResult
        {
            std::string reason;

            bool operator==(const FailureResult&) const = default;
        };

        using TranscodeDecisionResult = std::variant<DirectPlayResult, TranscodeResult, FailureResult>;
        TranscodeDecisionResult computeTranscodeDecision(const ClientInfo& clientInfo, const audio::AudioProperties& source);
    } // namespace details
} // namespace lms::api::subsonic
