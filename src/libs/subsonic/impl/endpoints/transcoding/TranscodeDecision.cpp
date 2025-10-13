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

#include "TranscodeDecision.hpp"

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "audio/AudioTypes.hpp"

#include "SubsonicResponse.hpp"
#include "responses/ClientInfo.hpp"

namespace lms::api::subsonic::details
{
    namespace
    {
        struct SupportedOutputFormat
        {
            std::string_view container;
            std::string_view audioCodec;
            bool isLossLess;
        };
        constexpr std::array<SupportedOutputFormat, 5> supportedOutputFormats{
            {
                { "mp3", "mp3", false },
                { "ogg", "opus", false },
                { "matroska", "opus", false },
                { "ogg", "vorbis", false },
                { "webm", "vorbis", false },
            }
        };

        struct AdjustResult
        {
            enum class Type
            {
                None,
                Adjusted,
                CannotAdjust,
            };
            Type type{ Type::None };
            std::optional<unsigned> newValue;
        };
        static AdjustResult adjustUsingLimitation(Limitation::ComparisonOperator comparisonOp, std::span<const std::string> values, unsigned originalValue)
        {
            switch (comparisonOp)
            {
            case Limitation::ComparisonOperator::Equals:
                {
                    assert(values.size() == 1);

                    const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
                    assert(value);
                    if (originalValue == *value)
                        return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };

                    // don't really know what to do here
                    return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
                }

            case Limitation::ComparisonOperator::NotEquals:
                {
                    assert(values.size() == 1);

                    const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
                    assert(value);
                    if (originalValue != *value)
                        return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };

                    // don't really know what to do here
                    return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
                }

            case Limitation::ComparisonOperator::LessThanEqual:
                {
                    assert(values.size() == 1);
                    const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
                    assert(value);
                    if (originalValue <= *value)
                        return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };

                    return AdjustResult{ .type = AdjustResult::Type::Adjusted, .newValue = *value };
                }

            case Limitation::ComparisonOperator::GreaterThanEqual:
                {
                    assert(values.size() == 1);
                    const auto value{ core::stringUtils::readAs<unsigned>(values.front()) };
                    assert(value);

                    if (originalValue >= *value)
                        return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };

                    // We don't want to use a higher value than the original one (we don't want to upscale)
                    return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = *value };
                }

            case Limitation::ComparisonOperator::EqualsAny:
                assert(!values.empty());
                // get the closest allowed value *below* originalValue (we don't want to upscale)
                {
                    std::optional<unsigned> closestValue;
                    for (std::string_view valueStr : values)
                    {
                        const auto value{ core::stringUtils::readAs<unsigned>(valueStr) };
                        assert(value);
                        if (*value == originalValue)
                            return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
                        if (*value < originalValue && (!closestValue || *value > *closestValue))
                            closestValue = *value;
                    }
                    if (closestValue)
                        return AdjustResult{ .type = AdjustResult::Type::Adjusted, .newValue = *closestValue };

                    // don't really know what to do here
                    return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
                }

            case Limitation::ComparisonOperator::NotEqualsAny:
                assert(!values.empty());
                {
                    if (std::none_of(std::cbegin(values), std::cend(values), [&](std::string_view valueStr) {
                            const auto value{ core::stringUtils::readAs<unsigned>(valueStr) };
                            assert(value);
                            return *value == originalValue;
                        }))
                    {
                        return AdjustResult{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
                    }

                    // don't really know what to do here
                    return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
                }
            }

            throw InternalErrorGenericError{ "Unhandled limitation comparison operator" };
        }

        static bool isStreamCompatibleWithLimitation(const StreamDetails& streamDetails, const Limitation& limitation)
        {
            if (!limitation.required)
                return true;

            // TODO handle strings
            std::optional<unsigned> valueToCheck{};
            switch (limitation.name)
            {
            case Limitation::Type::AudioBitrate:
                valueToCheck = static_cast<unsigned>(*streamDetails.audioBitrate);
                break;
            case Limitation::Type::AudioChannels:
                valueToCheck = static_cast<unsigned>(*streamDetails.audioChannels);
                break;
                break;
            case Limitation::Type::AudioSamplerate:
                valueToCheck = static_cast<unsigned>(*streamDetails.audioSamplerate);
                break;
            case Limitation::Type::AudioProfile:
                // TODO;
                break;
            case Limitation::Type::AudioBitdepth:
                if (streamDetails.audioBitdepth)
                    valueToCheck = static_cast<unsigned>(*streamDetails.audioBitdepth);
                break;
            }

            if (!valueToCheck)
                return false;

            const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *valueToCheck) };
            return adjustResult.type == AdjustResult::Type::None;
        }

        const CodecProfile* getAudioCodecProfile(std::span<const CodecProfile> codecProfiles, std::string_view codecName)
        {
            for (const CodecProfile& profile : codecProfiles)
            {
                if (profile.type != "AudioCodec")
                    continue;

                if (profile.name == "*" || core::stringUtils::stringCaseInsensitiveEqual(profile.name, codecName))
                    return &profile;
            }
            return nullptr;
        }

        std::optional<TranscodeReason> needsTranscode(const DirectPlayProfile& profile, std::span<const CodecProfile> codecProfiles, const StreamDetails& sourceStream)
        {
            if (profile.container != "*" && !core::stringUtils::stringCaseInsensitiveEqual(profile.container, sourceStream.container))
                return TranscodeReason::ContainerNotSupported;

            assert(!profile.audioCodecs.empty());
            if (profile.audioCodecs.front() != "*"
                && std::none_of(std::cbegin(profile.audioCodecs), std::cend(profile.audioCodecs), [&](const std::string& audioCodec) { return core::stringUtils::stringCaseInsensitiveEqual(audioCodec, sourceStream.codec); }))
                return TranscodeReason::AudioCodecNotSupported;

            if (profile.maxAudioChannels && *sourceStream.audioChannels > *profile.maxAudioChannels)
                return TranscodeReason::AudioChannelsNotSupported;

            if (profile.protocol != "*" && profile.protocol != "http")
                return TranscodeReason::ProtocolNotSupported;

            // check potential codec profiles limitations
            if (const CodecProfile * codecProfile{ getAudioCodecProfile(codecProfiles, sourceStream.codec) })
            {
                for (const Limitation& limitation : codecProfile->limitations)
                {
                    if (!isStreamCompatibleWithLimitation(sourceStream, limitation))
                    {
                        switch (limitation.name)
                        {
                        case Limitation::Type::AudioBitrate:
                            return TranscodeReason::AudioBitrateNotSupported;
                        case Limitation::Type::AudioChannels:
                            return TranscodeReason::AudioChannelsNotSupported;
                        case Limitation::Type::AudioSamplerate:
                            return TranscodeReason::AudioSampleRateNotSupported;
                        case Limitation::Type::AudioProfile:
                            return TranscodeReason::AudioCodecNotSupported;
                        case Limitation::Type::AudioBitdepth:
                            return TranscodeReason::AudioBitdepthNotSupported;
                        }
                    }
                }
            }

            return std::nullopt;
        }

        bool applyLimitation(const StreamDetails& sourceStream, const Limitation& limitation, StreamDetails& transcodedStream)
        {
            switch (limitation.name)
            {
            case Limitation::Type::AudioChannels:
                {
                    // transcodedStream.audioChannels may already be set by the transcoding profile maxAudioChannels
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, transcodedStream.audioChannels ? *transcodedStream.audioChannels : *sourceStream.audioChannels) };
                    if (adjustResult.type == AdjustResult::Type::CannotAdjust)
                        return false;
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioChannels = *adjustResult.newValue;
                }
                return true;

            case Limitation::Type::AudioBitrate:
                {
                    // TODO handle lossless output formats (cannot handle max bitrate limitation)
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, transcodedStream.audioBitrate ? *transcodedStream.audioBitrate : *sourceStream.audioBitrate) };
                    if (adjustResult.type == AdjustResult::Type::CannotAdjust)
                        return false;
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioBitrate = *adjustResult.newValue;
                }
                return true;

            case Limitation::Type::AudioProfile:
                // TODO
                return true;

            case Limitation::Type::AudioSamplerate:
                {
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *sourceStream.audioSamplerate) };
                    if (adjustResult.type == AdjustResult::Type::CannotAdjust)
                        return false;
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioSamplerate = *adjustResult.newValue;
                }
                return true;

            case Limitation::Type::AudioBitdepth:
                {
                    if (!sourceStream.audioBitdepth)
                        return false;

                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *sourceStream.audioBitdepth) };
                    if (adjustResult.type == AdjustResult::Type::CannotAdjust)
                        return false;
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioBitdepth = *adjustResult.newValue;
                }

                return true;
            }

            return false;
        }

        bool isLossless(std::string_view codecName)
        {
            static const std::array<std::string_view, 2> losslessAudioCodecs{ "alac", "flac" };
            return std::any_of(std::cbegin(losslessAudioCodecs), std::cend(losslessAudioCodecs), [&](std::string_view losslessCodec) { return core::stringUtils::stringCaseInsensitiveEqual(losslessCodec, codecName); });
        }

        std::optional<StreamDetails> computeTranscodedStream(std::optional<std::size_t> maxAudioBitrate, const TranscodingProfile& profile, std::span<const CodecProfile> codecProfiles, const StreamDetails& sourceStream)
        {
            if (profile.protocol != "http")
                return std::nullopt;

            // Find a supported output format
            const SupportedOutputFormat* outputFormat{ std::find_if(std::cbegin(supportedOutputFormats), std::cend(supportedOutputFormats), [&](const SupportedOutputFormat& format) {
                return format.container == profile.container && format.audioCodec == profile.audioCodec;
            }) };
            if (!outputFormat)
                return std::nullopt;

            StreamDetails transcodedStream;
            transcodedStream.protocol = "http";
            transcodedStream.container = outputFormat->container;
            transcodedStream.codec = outputFormat->audioCodec;

            if (isLossless(sourceStream.codec) && !isLossless(transcodedStream.codec))
            {
                // If coming from lossless source, maximize the bitrate if going to a non lossless source
                // otherwise, pick a good enough value as we don't want to keep the original bitrate which does not make sense for lossy codecs
                if (maxAudioBitrate)
                    transcodedStream.audioBitrate = maxAudioBitrate;
                else
                    transcodedStream.audioBitrate = 256'000; // TODO, only if no bitrate limitation found? take channel count into account?
            }
            else
            {
                // let's pick the same bitrate as the lossless source
                transcodedStream.audioBitrate = *sourceStream.audioBitrate;
            }

            if (maxAudioBitrate && *sourceStream.audioBitrate > *maxAudioBitrate)
                transcodedStream.audioBitrate = *maxAudioBitrate;

            if (profile.maxAudioChannels && *sourceStream.audioChannels > *profile.maxAudioChannels)
                transcodedStream.audioChannels = *profile.maxAudioChannels;

            if (const CodecProfile * codecProfile{ getAudioCodecProfile(codecProfiles, outputFormat->audioCodec) })
            {
                for (const Limitation& limitation : codecProfile->limitations)
                {
                    if (!applyLimitation(sourceStream, limitation, transcodedStream))
                        return std::nullopt;
                }
            }

            return transcodedStream;
        }

        bool canDirectPlay(const ClientInfo& clientInfo, const StreamDetails& sourceStream, std::vector<TranscodeReason>& transcodeReasons)
        {
            assert(sourceStream.audioBitrate);
            
            // Check global constraints
            if (clientInfo.maxAudioBitrate && *clientInfo.maxAudioBitrate < *sourceStream.audioBitrate)
            {
                transcodeReasons.push_back(TranscodeReason::AudioBitrateNotSupported);
                return false;
            }

            // Check direct play profiles
            for (const DirectPlayProfile& profile : clientInfo.directPlayProfiles)
            {
                const std::optional<TranscodeReason> transcodeReason{ needsTranscode(profile, clientInfo.codecProfiles, sourceStream) };
                if (!transcodeReason)
                    return true;

                transcodeReasons.push_back(*transcodeReason);
            }

            return false;
        }
    } // namespace

    TranscodeDecisionResult computeTranscodeDecision(const ClientInfo& clientInfo, const StreamDetails& sourceStream)
    {
        std::vector<TranscodeReason> transcodeReasons;

        if (canDirectPlay(clientInfo, sourceStream, transcodeReasons))
            return DirectPlayResult{};

        LMS_LOG(API_SUBSONIC, DEBUG, "Direct play not possible: no compatible direct play profile found");

        // Check transcoding profiles, we have to select the first one we can handle, order is important
        for (const TranscodingProfile& profile : clientInfo.transcodingProfiles)
        {
            std::optional<StreamDetails> targetStream{ computeTranscodedStream(clientInfo.maxTranscodingAudioBitrate, profile, clientInfo.codecProfiles, sourceStream) };
            if (targetStream)
                return TranscodeResult{ .reasons = transcodeReasons, .targetStreamInfo = *targetStream };
        }

        return FailureResult{ "No compatible direct play or transcoding profile found" };
    }
} // namespace lms::api::subsonic::details
