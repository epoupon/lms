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

#include "Utils.hpp"
#include "core/String.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"
#include "core/media/ContainerCodec.hpp"

#include "audio/Exception.hpp"

#include "Exception.hpp"

namespace lms::audio::ffmpeg::utils
{
    namespace
    {
        std::atomic<bool> isInitialized{ false };

        core::LiteralString avLogLevelToStr(int level)
        {
            switch (level)
            {
            case AV_LOG_TRACE:
                return "trace";
            case AV_LOG_DEBUG:
                return "debug";
            case AV_LOG_VERBOSE:
                return "verbose";
            case AV_LOG_INFO:
                return "info";
            case AV_LOG_WARNING:
                return "warning";
            case AV_LOG_ERROR:
                return "error";
            case AV_LOG_FATAL:
                return "fatal";
            case AV_LOG_PANIC:
                return "panic";
            default:
                return "unknown";
            }
        }

        void avLogCallback(void*, int level, const char* fmt, va_list vl)
        {
            if (!core::Service<core::logging::ILogger>::get() || core::Service<core::logging::ILogger>::get()->isSeverityActive(core::logging::Severity::DEBUG))
                return;

            if (level > AV_LOG_WARNING)
                return;

            std::array<char, 256> buffer{ 0 };
            if (std::vsnprintf(buffer.data(), buffer.size(), fmt, vl) > 0)
            {
                std::string_view str{ buffer.data() };
                str = core::stringUtils::stringTrimEnd(str, " \t\r\n");

                // TODO translate levels?
                LMS_LOG(AUDIO, DEBUG, "[FFmpeg] [" << avLogLevelToStr(level) << "] " << str);
            }
        }

        // When several registered muxers/encoders classify to the same Container/Codec, pick a specific,
        // deliberate one instead of depending on ffmpeg's internal iteration order
        constexpr std::array preferredMuxerNames{
            std::pair{ core::media::Container::ASF, std::string_view{ "asf" } }, // not "asf_stream"
            std::pair{ core::media::Container::Ogg, std::string_view{ "ogg" } }, // not "oga"/"ogv"/"opus"/"spx"
        };

        constexpr std::array preferredEncoderNames{
            std::pair{ core::media::Codec::AC3, std::string_view{ "ac3" } },          // not "ac3_fixed"
            std::pair{ core::media::Codec::MP3, std::string_view{ "libmp3lame" } },   // not "libshine"
            std::pair{ core::media::Codec::Opus, std::string_view{ "libopus" } },     // not the experimental native "opus"
            std::pair{ core::media::Codec::Vorbis, std::string_view{ "libvorbis" } }, // not the experimental native "vorbis"
        };

        template<typename Key, typename Table>
        std::optional<std::string_view> findPreferredName(const Table& table, Key key)
        {
            const auto it{ std::find_if(std::cbegin(table), std::cend(table), [&](const auto& entry) { return entry.first == key; }) };
            return it != std::cend(table) ? std::optional{ it->second } : std::nullopt;
        }

        class AvCapabilities
        {
        public:
            AvCapabilities()
            {
                ::av_log_set_callback(avLogCallback);

                void* opaque{};
                while (const AVInputFormat * inputFormat{ ::av_demuxer_iterate(&opaque) })
                {
                    if (const auto c{ containerFromFormatName(inputFormat->name) })
                        _supportedDemuxers.insert(*c);
                }

                opaque = nullptr;
                while (const AVOutputFormat * outputFormat{ ::av_muxer_iterate(&opaque) })
                {
                    if (const auto container{ containerFromFormatName(outputFormat->name) })
                    {
                        _supportedMuxers.insert(*container);
                        // Some containers (Ogg) register a distinct muxer name per codec (ogg/opus/spx/...), each only self-reporting its own default codec via avformat_query_codec: keep them
                        // all, and resolve which one to use per call site (see getMuxerNameForContainer and isCodecMuxingSupported below), instead of collapsing to one choice up front.
                        _muxersByContainer.emplace(*container, outputFormat);
                    }
                }

                opaque = nullptr;
                while (const AVCodec * avCodec{ ::av_codec_iterate(&opaque) })
                {
                    if (avCodec->type != AVMEDIA_TYPE_AUDIO)
                        continue;

                    const auto codec{ codecFromAVCodecId(avCodec->id) };
                    if (!codec)
                        continue;

                    if (::av_codec_is_decoder(avCodec))
                        _supportedDecoders.insert(*codec);

                    if (::av_codec_is_encoder(avCodec) && *codec != core::media::Codec::PCM) // PCM has one AVCodecID per bit depth/endianness combination, not handled
                    {
                        _supportedEncoders.insert(*codec);

                        if (const auto preferred{ findPreferredName(preferredEncoderNames, *codec) })
                        {
                            if (*preferred == avCodec->name)
                                _encoderByCodec[*codec] = avCodec;
                        }
                        else
                        {
                            _encoderByCodec.try_emplace(*codec, avCodec);
                        }
                    }
                }

                _supportedDemuxerExtensions = buildSupportedDemuxerExtensions();

                auto containerName{ [](core::media::Container c) { return std::string{ core::media::containerToString(c).str() }; } };
                auto codecName{ [](core::media::Codec c) { return std::string{ core::media::getCodecDesc(c).name.str() }; } };
                auto extensionName{ [](const std::filesystem::path& p) { return p.string(); } };

                LMS_LOG(AUDIO, INFO, "Supported demuxers: " << core::stringUtils::joinStrings(_supportedDemuxers | std::views::transform(containerName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported muxers: " << core::stringUtils::joinStrings(_supportedMuxers | std::views::transform(containerName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported decoders: " << core::stringUtils::joinStrings(_supportedDecoders | std::views::transform(codecName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported encoders: " << core::stringUtils::joinStrings(_supportedEncoders | std::views::transform(codecName), ' '));
                LMS_LOG(AUDIO, INFO, "Supported demuxer extensions: " << core::stringUtils::joinStrings(_supportedDemuxerExtensions | std::views::transform(extensionName), ' '));
            }

            bool isDemuxingSupported(core::media::Container container) const { return _supportedDemuxers.contains(container); }
            bool isDecodingSupported(core::media::Codec codec) const { return _supportedDecoders.contains(codec); }
            bool isMuxingSupported(core::media::Container container) const { return _supportedMuxers.contains(container); }
            bool isEncodingSupported(core::media::Codec codec) const { return _supportedEncoders.contains(codec); }

            std::span<const std::filesystem::path> getSupportedDemuxerExtensions() const { return _supportedDemuxerExtensions; }

            const AVCodec* getEncoderForCodec(core::media::Codec codec) const
            {
                const auto it{ _encoderByCodec.find(codec) };
                return it != _encoderByCodec.end() ? it->second : nullptr;
            }

            const char* getMuxerNameForContainer(core::media::Container container) const
            {
                const auto [rangeBegin, rangeEnd]{ _muxersByContainer.equal_range(container) };
                if (rangeBegin == rangeEnd)
                    return nullptr;

                if (const auto preferred{ findPreferredName(preferredMuxerNames, container) })
                {
                    const auto it{ std::find_if(rangeBegin, rangeEnd, [&](const auto& entry) { return *preferred == entry.second->name; }) };
                    if (it != rangeEnd)
                        return it->second->name;
                }

                return rangeBegin->second->name;
            }

            bool isCodecMuxingSupported(core::media::Container container, core::media::Codec codec) const
            {
                const auto [rangeBegin, rangeEnd]{ _muxersByContainer.equal_range(container) };
                if (rangeBegin == rangeEnd)
                    return false;

                const auto encoderIt{ _encoderByCodec.find(codec) };
                if (encoderIt == _encoderByCodec.end())
                    return false;

                const AVCodecID codecId{ encoderIt->second->id };

                for (auto it{ rangeBegin }; it != rangeEnd; ++it)
                {
                    if (::avformat_query_codec(it->second, codecId, FF_COMPLIANCE_NORMAL) > 0)
                        return true;
                }

                return false;
            }

        private:
            [[nodiscard]] std::vector<std::filesystem::path> buildSupportedDemuxerExtensions() const
            {
                std::vector<std::filesystem::path> result;

                core::media::visitContainerCodecPairs([&](const core::media::ContainerCodec& pair) {
                    if (!isDemuxingSupported(pair.container))
                        return;

                    for (const std::string_view extension : pair.extensions)
                    {
                        if (std::find(std::cbegin(result), std::cend(result), extension) == std::cend(result))
                            result.emplace_back(extension);
                    }
                });

                return result;
            }

            std::unordered_set<core::media::Container> _supportedDemuxers;
            std::unordered_set<core::media::Container> _supportedMuxers;
            std::unordered_set<core::media::Codec> _supportedDecoders;
            std::unordered_set<core::media::Codec> _supportedEncoders;
            std::vector<std::filesystem::path> _supportedDemuxerExtensions;
            std::unordered_multimap<core::media::Container, const AVOutputFormat*> _muxersByContainer;
            std::unordered_map<core::media::Codec, const AVCodec*> _encoderByCodec;
        };

        const AvCapabilities& getCapabilities()
        {
            static const AvCapabilities instance;
            return instance;
        }
    } // namespace

    std::string averrorToString(int error)
    {
        std::array<char, 128> buf{ 0 };

        if (::av_strerror(error, buf.data(), buf.size()) == 0)
            return buf.data();

        return "Unknown error";
    }

    std::optional<core::media::Container> containerFromFormatName(std::string_view name)
    {
        if (name == "aiff" || name == "aifc" || name == "aif")
            return core::media::Container::AIFF;
        if (name == "ape")
            return core::media::Container::APE;
        if (name.starts_with("asf"))
            return core::media::Container::ASF;
        if (name == "dsf")
            return core::media::Container::DSF;
        if (name == "flac")
            return core::media::Container::FLAC;
        if (name.find("mp4") != std::string_view::npos || name == "mov") // "mov" is a separate muxer name, same underlying format family
            return core::media::Container::MP4;
        if (name.starts_with("mpc"))
            return core::media::Container::MPC;
        if (name == "mp3" || name == "aac") // raw ADTS AAC has no container of its own; taglib bundles it under MPEG too
            return core::media::Container::MPEG;
        if (name == "ogg" || name == "oga" || name == "opus") // "oga"/"opus" are separate, codec-specific Ogg muxer names (default FLAC/Opus respectively)
            return core::media::Container::Ogg;
        if (name == "shn")
            return core::media::Container::Shorten;
        if (name == "tta")
            return core::media::Container::TrueAudio;
        if (name == "wav")
            return core::media::Container::WAV;
        if (name == "wv")
            return core::media::Container::WavPack;

        return std::nullopt;
    }

    std::optional<core::media::Codec> codecFromAVCodecId(AVCodecID codec)
    {
        switch (codec)
        {
        case AV_CODEC_ID_AAC:
            return core::media::Codec::AAC;
        case AV_CODEC_ID_AC3:
            return core::media::Codec::AC3;
        case AV_CODEC_ID_ALAC:
            return core::media::Codec::ALAC;
        case AV_CODEC_ID_APE:
            return core::media::Codec::APE;
        case AV_CODEC_ID_DSD_LSBF:
        case AV_CODEC_ID_DSD_LSBF_PLANAR:
        case AV_CODEC_ID_DSD_MSBF:
        case AV_CODEC_ID_DSD_MSBF_PLANAR:
            return core::media::Codec::DSD;
        case AV_CODEC_ID_EAC3:
            return core::media::Codec::EAC3;
        case AV_CODEC_ID_FLAC:
            return core::media::Codec::FLAC;
        case AV_CODEC_ID_MP3:
            return core::media::Codec::MP3;
        case AV_CODEC_ID_MP4ALS:
            return core::media::Codec::MP4ALS;
        case AV_CODEC_ID_MUSEPACK7:
            return core::media::Codec::MPC7;
        case AV_CODEC_ID_MUSEPACK8:
            return core::media::Codec::MPC8;
        case AV_CODEC_ID_OPUS:
            return core::media::Codec::Opus;
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_S16BE:
        case AV_CODEC_ID_PCM_U16LE:
        case AV_CODEC_ID_PCM_U16BE:
        case AV_CODEC_ID_PCM_S8:
        case AV_CODEC_ID_PCM_U8:
        case AV_CODEC_ID_PCM_S32LE:
        case AV_CODEC_ID_PCM_S32BE:
        case AV_CODEC_ID_PCM_U32LE:
        case AV_CODEC_ID_PCM_U32BE:
        case AV_CODEC_ID_PCM_S24LE:
        case AV_CODEC_ID_PCM_S24BE:
        case AV_CODEC_ID_PCM_U24LE:
        case AV_CODEC_ID_PCM_U24BE:
        case AV_CODEC_ID_PCM_S16LE_PLANAR:
        case AV_CODEC_ID_PCM_F32BE:
        case AV_CODEC_ID_PCM_F32LE:
        case AV_CODEC_ID_PCM_F64BE:
        case AV_CODEC_ID_PCM_F64LE:
        case AV_CODEC_ID_PCM_S8_PLANAR:
        case AV_CODEC_ID_PCM_S24LE_PLANAR:
        case AV_CODEC_ID_PCM_S32LE_PLANAR:
        case AV_CODEC_ID_PCM_S16BE_PLANAR:
        case AV_CODEC_ID_PCM_S64LE:
        case AV_CODEC_ID_PCM_S64BE:
        case AV_CODEC_ID_PCM_F16LE:
        case AV_CODEC_ID_PCM_F24LE:
        case AV_CODEC_ID_PCM_MULAW:
        case AV_CODEC_ID_PCM_ALAW:
        case AV_CODEC_ID_ADPCM_G726:
        case AV_CODEC_ID_ADPCM_G722:
        case AV_CODEC_ID_ADPCM_G726LE:
            return core::media::Codec::PCM;
        case AV_CODEC_ID_SHORTEN:
            return core::media::Codec::Shorten;
        case AV_CODEC_ID_TTA:
            return core::media::Codec::TrueAudio;
        case AV_CODEC_ID_VORBIS:
            return core::media::Codec::Vorbis;
        case AV_CODEC_ID_WAVPACK:
            return core::media::Codec::WavPack;
        case AV_CODEC_ID_WMALOSSLESS:
            return core::media::Codec::WMA9Lossless;
        case AV_CODEC_ID_WMAPRO:
            return core::media::Codec::WMA9Pro;
        case AV_CODEC_ID_WMAV1:
            return core::media::Codec::WMA1;
        case AV_CODEC_ID_WMAV2:
            return core::media::Codec::WMA2;

        default:
            return std::nullopt;
        }
    }

    std::span<const std::filesystem::path> getSupportedDemuxerExtensions()
    {
        return getCapabilities().getSupportedDemuxerExtensions();
    }

    bool isDemuxingSupported(core::media::Container container)
    {
        return getCapabilities().isDemuxingSupported(container);
    }

    bool isDecodingSupported(core::media::Codec codec)
    {
        return getCapabilities().isDecodingSupported(codec);
    }

    bool isMuxingSupported(core::media::Container container)
    {
        return getCapabilities().isMuxingSupported(container);
    }

    bool isEncodingSupported(core::media::Codec codec)
    {
        return getCapabilities().isEncodingSupported(codec);
    }

    bool isCodecMuxingSupported(core::media::Container container, core::media::Codec codec)
    {
        return getCapabilities().isCodecMuxingSupported(container, codec);
    }

    const AVCodec* getEncoderForCodec(core::media::Codec codec)
    {
        return getCapabilities().getEncoderForCodec(codec);
    }

    const char* getMuxerNameForContainer(core::media::Container container)
    {
        return getCapabilities().getMuxerNameForContainer(container);
    }

    PcmSampleType toPcmSampleType(::AVSampleFormat format)
    {
        switch (format)
        {
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return PcmSampleType::Signed16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return PcmSampleType::Signed32;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return PcmSampleType::Float32;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            return PcmSampleType::Float64;

        default:
            break;
        }

        throw Exception{ "Unsupported sample format " + std::string{ ::av_get_sample_fmt_name(format) ? ::av_get_sample_fmt_name(format) : "?" } };
    }

    ::AVSampleFormat toAvSampleFormat(PcmSampleType type, bool planar)
    {
        switch (type)
        {
        case PcmSampleType::Signed16:
            return planar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
        case PcmSampleType::Signed32:
            return planar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
        case PcmSampleType::Float32:
            return planar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
        case PcmSampleType::Float64:
            return planar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
        }

        throw Exception{ "Unsupported PcmSampleType" };
    }

    void init()
    {
        assert(!isInitialized);

        LMS_LOG(AUDIO, INFO, "Initializing ffmpeg backend...");
        isInitialized = true;
        getCapabilities(); // init
        LMS_LOG(AUDIO, INFO, "ffmpeg backend init done");
    }

    bool isInit()
    {
        return isInitialized;
    }
} // namespace lms::audio::ffmpeg::utils