/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "EncoderUtils.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}

#include "audio/Exception.hpp"

#include "Exception.hpp"
#include "Utils.hpp"

namespace lms::audio::ffmpeg::utils
{
    namespace
    {
        std::string codecToString(core::media::Codec codec)
        {
            return std::string{ core::media::getCodecDesc(codec).name.str() };
        }

        template<typename T>
        std::span<const T> getSupportedConfig(const AVCodec& encoder, ::AVCodecConfig config)
        {
            const void* values{};
            int count{};
            if (::avcodec_get_supported_config(nullptr, &encoder, config, 0, &values, &count) < 0 || !values)
                return {};

            return std::span<const T>{ static_cast<const T*>(values), static_cast<std::size_t>(count) };
        }

        // An empty result means the encoder does not restrict this configuration
        std::vector<::AVSampleFormat> getSupportedSampleFormats(const AVCodec& encoder)
        {
            std::vector<::AVSampleFormat> res;

            const std::span<const ::AVSampleFormat> formats{ getSupportedConfig<::AVSampleFormat>(encoder, AV_CODEC_CONFIG_SAMPLE_FORMAT) };
            res.assign(std::cbegin(formats), std::cend(formats));

            return res;
        }

        std::vector<int> getSupportedSampleRates(const AVCodec& encoder)
        {
            std::vector<int> res;

            const std::span<const int> sampleRates{ getSupportedConfig<int>(encoder, AV_CODEC_CONFIG_SAMPLE_RATE) };
            res.assign(std::cbegin(sampleRates), std::cend(sampleRates));

            return res;
        }

        std::vector<const AVChannelLayout*> getSupportedChannelLayouts(const AVCodec& encoder)
        {
            std::vector<const AVChannelLayout*> res;

            for (const AVChannelLayout& layout : getSupportedConfig<AVChannelLayout>(encoder, AV_CODEC_CONFIG_CHANNEL_LAYOUT))
                res.push_back(&layout);

            return res;
        }

        // From the most to the least desirable, assuming we never want to lose precision if we can avoid it
        std::array<PcmSampleType, 4> getSampleTypePreferences(PcmSampleType desiredSampleType)
        {
            switch (desiredSampleType)
            {
            case PcmSampleType::Signed16:
                return { PcmSampleType::Signed16, PcmSampleType::Signed32, PcmSampleType::Float32, PcmSampleType::Float64 };
            case PcmSampleType::Signed32:
                return { PcmSampleType::Signed32, PcmSampleType::Float64, PcmSampleType::Float32, PcmSampleType::Signed16 };
            case PcmSampleType::Float32:
                return { PcmSampleType::Float32, PcmSampleType::Float64, PcmSampleType::Signed32, PcmSampleType::Signed16 };
            case PcmSampleType::Float64:
                return { PcmSampleType::Float64, PcmSampleType::Float32, PcmSampleType::Signed32, PcmSampleType::Signed16 };
            }

            throw Exception{ "Unsupported PcmSampleType" };
        }
    } // namespace

    const char* getMuxerName(core::media::Container container)
    {
        const char* name{ getMuxerNameForContainer(container) };
        if (!name)
            throw Exception{ "Container type " + std::string{ core::media::containerToString(container).str() } + " is not supported by this build" };

        return name;
    }

    const AVCodec* findEncoder(core::media::Codec codec)
    {
        const AVCodec* encoder{ getEncoderForCodec(codec) };
        if (!encoder)
            throw Exception{ "No encoder available for codec " + codecToString(codec) + ": check the FFmpeg libraries this build is linked against" };

        return encoder;
    }

    ::AVSampleFormat pickSampleFormat(const AVCodec& encoder, PcmSampleType desiredSampleType)
    {
        const std::vector<::AVSampleFormat> supportedFormats{ getSupportedSampleFormats(encoder) };
        if (supportedFormats.empty())
            return toAvSampleFormat(desiredSampleType, false);

        for (const PcmSampleType sampleType : getSampleTypePreferences(desiredSampleType))
        {
            for (const bool planar : { false, true })
            {
                const ::AVSampleFormat candidate{ toAvSampleFormat(sampleType, planar) };
                if (std::find(std::cbegin(supportedFormats), std::cend(supportedFormats), candidate) != std::cend(supportedFormats))
                    return candidate;
            }
        }

        throw Exception{ "Encoder '" + std::string{ encoder.name } + "' does not support any usable sample format" };
    }

    int pickSampleRate(const AVCodec& encoder, unsigned desiredSampleRate)
    {
        const std::vector<int> supportedSampleRates{ getSupportedSampleRates(encoder) };
        if (supportedSampleRates.empty())
            return static_cast<int>(desiredSampleRate);

        // Prefer not to downsample: pick the lowest supported rate that is high enough
        const int desired{ static_cast<int>(desiredSampleRate) };
        int best{};
        for (const int sampleRate : supportedSampleRates)
        {
            if (sampleRate >= desired && (best == 0 || sampleRate < best))
                best = sampleRate;
        }

        if (best != 0)
            return best;

        return *std::max_element(std::cbegin(supportedSampleRates), std::cend(supportedSampleRates));
    }

    void pickChannelLayout(const AVCodec& encoder, unsigned desiredChannelCount, AVChannelLayout& layout)
    {
        const std::vector<const AVChannelLayout*> supportedLayouts{ getSupportedChannelLayouts(encoder) };
        if (supportedLayouts.empty())
        {
            ::av_channel_layout_default(&layout, static_cast<int>(desiredChannelCount));
            return;
        }

        const int desired{ static_cast<int>(desiredChannelCount) };
        const AVChannelLayout* best{};
        int bestScore{};
        for (const AVChannelLayout* candidate : supportedLayouts)
        {
            // Prefer not to upmix: any layout narrower than requested beats any wider one
            const int score{ candidate->nb_channels <= desired ? desired - candidate->nb_channels : 1'000 + candidate->nb_channels - desired };
            if (!best || score < bestScore)
            {
                best = candidate;
                bestScore = score;
            }
        }

        const int error{ ::av_channel_layout_copy(&layout, best) };
        if (error < 0)
            throw FFmpegException{ "Cannot copy channel layout", error };
    }
} // namespace lms::audio::ffmpeg::utils
