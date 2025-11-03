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

#include <array>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "audio/AudioTypes.hpp"

#include "SubsonicResponse.hpp"
#include "responses/ClientInfo.hpp"

namespace lms::api::subsonic::details
{
    namespace
    {
        constexpr std::array<TranscodeFormat, 4> supportedTranscodeFormats{
            {
                { .container = audio::ContainerType::MPEG, .codec = audio::CodecType::MP3, .outputFormat = audio::OutputFormat::MP3 },
                { .container = audio::ContainerType::Ogg, .codec = audio::CodecType::Vorbis, .outputFormat = audio::OutputFormat::OGG_VORBIS },
                { .container = audio::ContainerType::Ogg, .codec = audio::CodecType::Opus, .outputFormat = audio::OutputFormat::OGG_OPUS },
                { .container = audio::ContainerType::FLAC, .codec = audio::CodecType::FLAC, .outputFormat = audio::OutputFormat::FLAC },
            }
        };

        bool isMatchingContainerName(audio::ContainerType container, std::string_view containerStr)
        {
            std::span<const std::string_view> containerNames;

            switch (container)
            {
            case audio::ContainerType::AIFF:
                containerNames = { { "aif", "aiff" } };
                break;
            case audio::ContainerType::APE:
                containerNames = { { "ape" } };
                break;
            case audio::ContainerType::ASF:
                containerNames = { { "asf", "wma" } };
                break;
            case audio::ContainerType::DSF:
                containerNames = { { "dsf" } };
                break;
            case audio::ContainerType::MPC:
                containerNames = { { "mpc", "mpp", "mp" } };
                break;
            case audio::ContainerType::MPEG:
                containerNames = { { "mp3", "mp2", "mpeg" } };
                break;
            case audio::ContainerType::Ogg:
                containerNames = { { "ogg", "oga" } };
                break;
            case audio::ContainerType::FLAC:
                containerNames = { { "flac" } };
                break;
            case audio::ContainerType::MP4:
                containerNames = { { "aac", "adts", "m4a", "mp4", "m4b", "m4p" } };
                break;
            case audio::ContainerType::Shorten:
                containerNames = { { "shn" } };
                break;
            case audio::ContainerType::TrueAudio:
                containerNames = { { "tta" } };
                break;
            case audio::ContainerType::WAV:
                containerNames = { { "wav" } };
                break;
            case audio::ContainerType::WavPack:
                containerNames = { { "wv" } };
                break;
            }

            return std::any_of(std::cbegin(containerNames), std::cend(containerNames), [&](std::string_view containerName) { return core::stringUtils::stringCaseInsensitiveEqual(containerName, containerStr); });
        }

        bool isMatchingCodecName(audio::CodecType codec, std::string_view codecStr)
        {
            std::span<const std::string_view> codecNames;

            switch (codec)
            {
            case audio::CodecType::AAC:
                codecNames = { { "aac", "adts" } };
                break;
            case audio::CodecType::ALAC:
                codecNames = { { "alac" } };
                break;
            case audio::CodecType::APE:
                codecNames = { { "ape" } };
                break;
            case audio::CodecType::DSD:
                codecNames = { { "dsd" } };
                break;
            case audio::CodecType::FLAC:
                codecNames = { { "flac" } };
                break;
            case audio::CodecType::MP3:
                codecNames = { { "mp3" } };
                break;
            case audio::CodecType::MP4ALS:
                codecNames = { { "mp4als", "als" } };
                break;
            case audio::CodecType::MPC7:
                codecNames = { { "mpc7", "musepack7" } };
                break;
            case audio::CodecType::MPC8:
                codecNames = { { "mpc8", "musepack8" } };
                break;
            case audio::CodecType::Opus:
                codecNames = { { "opus" } };
                break;
            case audio::CodecType::PCM:
                codecNames = { { "pcm" } };
                break;
            case audio::CodecType::Shorten:
                codecNames = { { "shn", "shorten" } };
                break;
            case audio::CodecType::TrueAudio:
                codecNames = { { "tta" } };
                break;
            case audio::CodecType::Vorbis:
                codecNames = { { "vorbis" } };
                break;
            case audio::CodecType::WavPack:
                codecNames = { { "wv" } };
                break;
            case audio::CodecType::WMA1:
                codecNames = { { "wma1", "wmav1" } };
                break;
            case audio::CodecType::WMA2:
                codecNames = { { "wma2", "wmav2" } };
                break;
            case audio::CodecType::WMA9Lossless:
                codecNames = { { "wmalossless", "wma9lossless" } };
                break;
            case audio::CodecType::WMA9Pro:
                codecNames = { { "wmapro", "wma9pro" } };
                break;
            }

            return std::any_of(std::cbegin(codecNames), std::cend(codecNames), [&](std::string_view codecName) { return core::stringUtils::stringCaseInsensitiveEqual(codecName, codecStr); });
        }

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

        static bool isStreamCompatibleWithLimitation(const audio::AudioProperties& source, const Limitation& limitation)
        {
            if (!limitation.required)
                return true;

            // TODO handle strings
            std::optional<unsigned> valueToCheck{};
            switch (limitation.name)
            {
            case Limitation::Type::AudioBitrate:
                valueToCheck = static_cast<unsigned>(source.bitrate);
                break;
            case Limitation::Type::AudioChannels:
                valueToCheck = static_cast<unsigned>(source.channelCount);
                break;
                break;
            case Limitation::Type::AudioSamplerate:
                valueToCheck = static_cast<unsigned>(source.sampleRate);
                break;
            case Limitation::Type::AudioProfile:
                // TODO;
                break;
            case Limitation::Type::AudioBitdepth:
                if (source.bitsPerSample)
                    valueToCheck = static_cast<unsigned>(*source.bitsPerSample);
                break;
            }

            if (!valueToCheck)
                return false;

            const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *valueToCheck) };
            return adjustResult.type == AdjustResult::Type::None;
        }

        const CodecProfile* getAudioCodecProfile(std::span<const CodecProfile> codecProfiles, audio::CodecType codec)
        {
            for (const CodecProfile& profile : codecProfiles)
            {
                if (profile.type != "AudioCodec")
                    continue;

                if (profile.name == "*" || isMatchingCodecName(codec, profile.name))
                    return &profile;
            }
            return nullptr;
        }

        std::optional<TranscodeReason> needsTranscode(const DirectPlayProfile& profile, std::span<const CodecProfile> codecProfiles, const audio::AudioProperties& source)
        {
            assert(!profile.containers.empty());
            if (profile.containers.front() != "*" && std::none_of(std::cbegin(profile.containers), std::cend(profile.containers), [&](const std::string& container) { return isMatchingContainerName(source.container, container); }))
                return TranscodeReason::ContainerNotSupported;

            assert(!profile.audioCodecs.empty());
            if (profile.audioCodecs.front() != "*" && std::none_of(std::cbegin(profile.audioCodecs), std::cend(profile.audioCodecs), [&](const std::string& audioCodec) { return isMatchingCodecName(source.codec, audioCodec); }))
                return TranscodeReason::AudioCodecNotSupported;

            if (profile.maxAudioChannels && source.channelCount > *profile.maxAudioChannels)
                return TranscodeReason::AudioChannelsNotSupported;

            if (profile.protocol != "*" && profile.protocol != "http")
                return TranscodeReason::ProtocolNotSupported;

            // check potential codec profiles limitations
            if (const CodecProfile * codecProfile{ getAudioCodecProfile(codecProfiles, source.codec) })
            {
                for (const Limitation& limitation : codecProfile->limitations)
                {
                    if (!isStreamCompatibleWithLimitation(source, limitation))
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

        AdjustResult applyLimitation(const audio::AudioProperties& source, const Limitation& limitation, StreamDetails& transcodedStream)
        {
            switch (limitation.name)
            {
            case Limitation::Type::AudioChannels:
                {
                    // transcodedStream.audioChannels may already be set by the transcoding profile maxAudioChannels
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, transcodedStream.audioChannels ? *transcodedStream.audioChannels : source.channelCount) };
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioChannels = *adjustResult.newValue;
                    return adjustResult;
                }

            case Limitation::Type::AudioBitrate:
                {
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, transcodedStream.audioBitrate ? *transcodedStream.audioBitrate : source.bitrate) };
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioBitrate = *adjustResult.newValue;
                    return adjustResult;
                }

            case Limitation::Type::AudioProfile:
                {
                    // TODO
                    AdjustResult res{ .type = AdjustResult::Type::None, .newValue = std::nullopt };
                    return res;
                }

            case Limitation::Type::AudioSamplerate:
                {
                    const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, source.sampleRate) };
                    if (adjustResult.type == AdjustResult::Type::Adjusted)
                        transcodedStream.audioSamplerate = *adjustResult.newValue;
                    return adjustResult;
                }

            case Limitation::Type::AudioBitdepth:
                {
                    if (source.bitsPerSample)
                    {
                        const AdjustResult adjustResult{ adjustUsingLimitation(limitation.comparison, limitation.values, *source.bitsPerSample) };
                        if (adjustResult.type == AdjustResult::Type::Adjusted)
                            transcodedStream.audioBitdepth = *adjustResult.newValue;

                        return adjustResult;
                    }
                }
            }

            return AdjustResult{ .type = AdjustResult::Type::CannotAdjust, .newValue = std::nullopt };
        }

        std::optional<StreamDetails> computeTranscodedStream(std::optional<std::size_t> maxAudioBitrate, const TranscodingProfile& profile, std::span<const CodecProfile> codecProfiles, const audio::AudioProperties& source)
        {
            if (profile.protocol != "http")
                return std::nullopt;

            const TranscodeFormat* transcodeFormat{ selectTranscodeFormat(profile.container, profile.audioCodec) };
            if (!transcodeFormat)
                return std::nullopt;

            StreamDetails transcodedStream;
            transcodedStream.protocol = "http";
            transcodedStream.container = profile.container; // put back what was requested instead of our internal names
            transcodedStream.codec = profile.audioCodec;    // put back what was requested instead of our internal names

            if (audio::isCodecLossless(source.codec))
            {
                if (!audio::isCodecLossless(transcodeFormat->codec))
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
                    // If going to a lossless codec, make sure we can respect the original bitrate
                    // technically, we could have a chance to respect the bitrate if we apply limitations, but that's not easy to have a strong garantee
                    if (maxAudioBitrate && source.bitrate > *maxAudioBitrate)
                        return std::nullopt;
                }
            }
            else
            {
                // source is lossy

                if (audio::isCodecLossless(transcodeFormat->codec))
                    return std::nullopt; // not compatible with lossless codecs

                // let's pick the same bitrate as the lossy source
                transcodedStream.audioBitrate = source.bitrate;
            }

            if (maxAudioBitrate && source.bitrate > *maxAudioBitrate)
                transcodedStream.audioBitrate = *maxAudioBitrate;

            if (profile.maxAudioChannels && source.channelCount > *profile.maxAudioChannels)
                transcodedStream.audioChannels = *profile.maxAudioChannels;

            if (const CodecProfile * codecProfile{ getAudioCodecProfile(codecProfiles, transcodeFormat->codec) })
            {
                for (const Limitation& limitation : codecProfile->limitations)
                {
                    const AdjustResult result{ applyLimitation(source, limitation, transcodedStream) };
                    if (limitation.name == Limitation::Type::AudioBitrate && audio::isCodecLossless(transcodeFormat->codec) && result.type == AdjustResult::Type::Adjusted)
                        return std::nullopt; // not compatible with lossless codecs

                    if (result.type == AdjustResult::Type::CannotAdjust)
                        return std::nullopt;
                }
            }

            return transcodedStream;
        }

        bool canDirectPlay(const ClientInfo& clientInfo, const audio::AudioProperties& source, std::vector<TranscodeReason>& transcodeReasons)
        {
            // Check global constraints
            if (clientInfo.maxAudioBitrate && *clientInfo.maxAudioBitrate < source.bitrate)
            {
                transcodeReasons.push_back(TranscodeReason::AudioBitrateNotSupported);
                return false;
            }

            // Check direct play profiles
            for (const DirectPlayProfile& profile : clientInfo.directPlayProfiles)
            {
                const std::optional<TranscodeReason> transcodeReason{ needsTranscode(profile, clientInfo.codecProfiles, source) };
                if (!transcodeReason)
                    return true;

                transcodeReasons.push_back(*transcodeReason);
            }

            return false;
        }
    } // namespace

    core::LiteralString transcodeReasonToString(TranscodeReason reason)
    {
        switch (reason)
        {
        case TranscodeReason::AudioCodecNotSupported:
            return "audio codec not supported";
        case TranscodeReason::AudioBitrateNotSupported:
            return "audio bitrate not supported";
        case TranscodeReason::AudioChannelsNotSupported:
            return "audio channels not supported";
        case TranscodeReason::AudioSampleRateNotSupported:
            return "audio samplerate not supported";
        case TranscodeReason::AudioBitdepthNotSupported:
            return "audio bitdepth not supported";
        case TranscodeReason::ContainerNotSupported:
            return "container not supported";
        case TranscodeReason::ProtocolNotSupported:
            return "protocol not supported";
        }

        return "unknown";
    }

    const TranscodeFormat* selectTranscodeFormat(std::string_view containerName, std::string_view codecName)
    {
        // Find a supported output format
        const auto it{ std::find_if(std::cbegin(supportedTranscodeFormats), std::cend(supportedTranscodeFormats), [&](const TranscodeFormat& format) {
            return isMatchingCodecName(format.codec, codecName) && isMatchingContainerName(format.container, containerName);
        }) };
        if (it == std::cend(supportedTranscodeFormats))
            return nullptr;

        return &(*it);
    }

    TranscodeDecisionResult computeTranscodeDecision(const ClientInfo& clientInfo, const audio::AudioProperties& source)
    {
        std::vector<TranscodeReason> transcodeReasons;

        if (canDirectPlay(clientInfo, source, transcodeReasons))
            return DirectPlayResult{};

        LMS_LOG(API_SUBSONIC, DEBUG, "Direct play not possible: no compatible direct play profile found");

        // Check transcoding profiles, we have to select the first one we can handle, order is important
        for (const TranscodingProfile& profile : clientInfo.transcodingProfiles)
        {
            std::optional<StreamDetails> targetStream{ computeTranscodedStream(clientInfo.maxTranscodingAudioBitrate, profile, clientInfo.codecProfiles, source) };
            if (targetStream)
                return TranscodeResult{ .reasons = transcodeReasons, .targetStreamInfo = *targetStream };
        }

        return FailureResult{ "No compatible direct play or transcoding profile found" };
    }
} // namespace lms::api::subsonic::details
