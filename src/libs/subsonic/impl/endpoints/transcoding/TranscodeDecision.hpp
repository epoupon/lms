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

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "core/LiteralString.hpp"

#include "audio/AudioProperties.hpp"
#include "audio/TranscodeTypes.hpp"

#include "payloads/StreamDetails.hpp"

namespace lms::api::subsonic
{
    struct ClientInfo;

    // Bitrate picked when encoding a lossless source to a lossy format and the client sets no limit
    inline constexpr std::size_t defaultLossyTranscodeBitrate{ 256'000 };

    // Ceiling on a lossy source bitrate reused as the target: re-encoding higher gains nothing,
    // and some encoders reject higher rates (libopus: 256 kbps per channel)
    inline constexpr std::size_t maxLossyToLossyTranscodeBitrate{ 256'000 };

    namespace detail
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

        core::LiteralString transcodeReasonToString(TranscodeReason reason);

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
        TranscodeDecisionResult computeTranscodeDecision(const ClientInfo& clientInfo, const audio::AudioProperties& source, std::size_t sourceFileBitrate);

        std::optional<audio::TranscodeOutputFormat> findAudioFormatByName(std::string_view containerName, std::string_view codecName);
        std::optional<audio::TranscodeOutputFormat> findSupportedTranscodeOutputFormat(std::string_view containerName, std::string_view codecName);
    } // namespace detail
} // namespace lms::api::subsonic
