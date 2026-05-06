/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "AudioFile.hpp"

#include <cstdio>
#include <unordered_map>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"

#include "Exception.hpp"
#include "Utils.hpp"

#define LMS_FFMPEG_HAS_AV_DICT_ITERATE (LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 37, 100))

namespace lms::audio::ffmpeg
{
    namespace
    {
        void extractMetaDataFromDictionnary(AVDictionary* dictionnary, AudioFile::MetadataMap& res)
        {
            if (!dictionnary)
                return;

#if LMS_FFMPEG_HAS_AV_DICT_ITERATE
            const AVDictionaryEntry* tag{ NULL };
            while ((tag = av_dict_iterate(dictionnary, tag)))
                res[core::stringUtils::stringToUpper(tag->key)] = tag->value;
#else
            AVDictionaryEntry* tag{};
            while ((tag = av_dict_get(dictionnary, "", tag, AV_DICT_IGNORE_SUFFIX)))
                res[core::stringUtils::stringToUpper(tag->key)] = tag->value;
#endif // LMS_FFMPEG_HAS_AV_DICT_ITERATE
        }

        std::optional<core::media::Container> avdemuxerToContainerType(std::string_view name)
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
            if (name.find("mp4") != std::string_view::npos)
                return core::media::Container::MP4;
            if (name.starts_with("mpc"))
                return core::media::Container::MPC;
            if (name == "mp3")
                return core::media::Container::MPEG;
            if (name == "ogg")
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

        std::optional<core::media::Codec> avcodecToCodecType(AVCodecID codec)
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
    } // namespace

    AudioFile::AudioFile(const std::filesystem::path& p)
        : _p{ p }
    {
        LMS_SCOPED_TRACE_DETAILED("MetaData", "FFmpegParseFile");

        utils::init();

        int error{ avformat_open_input(&_context, _p.c_str(), nullptr, nullptr) };
        if (error < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot open " << _p << ": " << utils::averrorToString(error));
            throw FFmpegException{ "Cannot open '" + _p.string() + "'", error };
        }

        error = avformat_find_stream_info(_context, nullptr);
        if (error < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot find stream information in " << _p << ": " << utils::averrorToString(error));
            avformat_close_input(&_context);
            throw FFmpegException{ "Cannot find stream information in '" + _p.string() + "'", error };
        }
    }

    AudioFile::~AudioFile()
    {
        avformat_close_input(&_context);
    }

    const std::filesystem::path& AudioFile::getPath() const
    {
        return _p;
    }

    ContainerInfo AudioFile::getContainerInfo() const
    {
        ContainerInfo info;

        info.container = avdemuxerToContainerType(_context->iformat->name);
        info.containerName = _context->iformat->name;

        if (_context->bit_rate > 0)
            info.bitrate = _context->bit_rate;
        info.duration = std::chrono::milliseconds{ _context->duration == AV_NOPTS_VALUE ? 0 : _context->duration / AV_TIME_BASE * 1'000 };

        return info;
    }

    AudioFile::MetadataMap AudioFile::extractMetaData() const
    {
        MetadataMap res;

        extractMetaDataFromDictionnary(_context->metadata, res);

        // HACK for OGG files
        // If we did not find tags, search metadata in streams
        if (res.empty())
        {
            for (std::size_t i{}; i < _context->nb_streams; ++i)
            {
                extractMetaDataFromDictionnary(_context->streams[i]->metadata, res);

                if (!res.empty())
                    break;
            }
        }

        return res;
    }

    std::vector<StreamInfo> AudioFile::getStreamInfo() const
    {
        std::vector<StreamInfo> res;

        for (std::size_t i{}; i < _context->nb_streams; ++i)
        {
            std::optional<StreamInfo> streamInfo{ getStreamInfo(i) };
            if (streamInfo)
                res.emplace_back(std::move(*streamInfo));
        }

        return res;
    }

    std::optional<std::size_t> AudioFile::getBestStreamIndex() const
    {
        int res = ::av_find_best_stream(_context,
                                        AVMEDIA_TYPE_AUDIO,
                                        -1, // Auto
                                        -1, // Auto
                                        NULL,
                                        0);

        if (res < 0)
            return std::nullopt;

        return res;
    }

    std::optional<StreamInfo> AudioFile::getBestStreamInfo() const
    {
        std::optional<StreamInfo> res;

        std::optional<std::size_t> bestStreamIndex{ getBestStreamIndex() };
        if (bestStreamIndex)
            res = getStreamInfo(*bestStreamIndex);

        return res;
    }

    bool AudioFile::hasAttachedPictures() const
    {
        for (std::size_t i{}; i < _context->nb_streams; ++i)
        {
            if (_context->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
                return true;
        }

        return false;
    }

    void AudioFile::visitAttachedPictures(std::function<void(const PictureView&, const MetadataMap&)> func) const
    {
        static const std::unordered_map<int, std::string> codecMimeMap{
            { AV_CODEC_ID_BMP, "image/bmp" },
            { AV_CODEC_ID_GIF, "image/gif" },
            { AV_CODEC_ID_MJPEG, "image/jpeg" },
            { AV_CODEC_ID_PNG, "image/png" },
            { AV_CODEC_ID_PPM, "image/x-portable-pixmap" },
        };

        for (std::size_t i{}; i < _context->nb_streams; ++i)
        {
            AVStream* avstream = _context->streams[i];

            // Skip attached pics
            if (!(avstream->disposition & AV_DISPOSITION_ATTACHED_PIC))
                continue;

            if (avstream->codecpar == nullptr)
            {
                LMS_LOG(AUDIO, WARNING, "Skipping stream " << i << " since no codecpar is set");
                continue;
            }

            MetadataMap metadata;
            extractMetaDataFromDictionnary(avstream->metadata, metadata);

            PictureView picture;

            auto itMime = codecMimeMap.find(avstream->codecpar->codec_id);
            if (itMime != codecMimeMap.end())
            {
                picture.mimeType = itMime->second;
            }
            else
            {
                picture.mimeType = "application/octet-stream";
                LMS_LOG(AUDIO, WARNING, "AVCodecID" << avstream->codecpar->codec_id << " (" << ::avcodec_get_name(avstream->codecpar->codec_id) << ") not handled in mime type conversion");
            }

            const ::AVPacket& pkt{ avstream->attached_pic };

            picture.data = std::span{ reinterpret_cast<const std::byte*>(pkt.data), static_cast<std::size_t>(pkt.size) };
            func(picture, metadata);
        }
    }

    std::optional<StreamInfo> AudioFile::getStreamInfo(std::size_t streamIndex) const
    {
        std::optional<StreamInfo> res;

        AVStream* avstream{ _context->streams[streamIndex] };
        assert(avstream);

        if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
            return res;

        if (!avstream->codecpar)
        {
            LMS_LOG(AUDIO, WARNING, "Skipping stream " << streamIndex << " since no codecpar is set");
            return res;
        }

        if (avstream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
            return res;

        res.emplace();

        res->index = streamIndex;
        res->codec = avcodecToCodecType(avstream->codecpar->codec_id);
        res->codecName = ::avcodec_get_name(avstream->codecpar->codec_id);

        if (avstream->codecpar->bit_rate)
            res->bitrate = static_cast<std::size_t>(avstream->codecpar->bit_rate);
        if (avstream->codecpar->bits_per_coded_sample)
            res->bitsPerSample = static_cast<std::size_t>(avstream->codecpar->bits_per_coded_sample);
        else if (avstream->codecpar->bits_per_raw_sample)
            res->bitsPerSample = static_cast<std::size_t>(avstream->codecpar->bits_per_raw_sample);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
        if (avstream->codecpar->channels)
            res->channelCount = static_cast<std::size_t>(avstream->codecpar->channels);
#else
        if (avstream->codecpar->ch_layout.nb_channels)
            res->channelCount = static_cast<std::size_t>(avstream->codecpar->ch_layout.nb_channels);
#endif
        assert(!res->codecName.empty()); // doc says it is never NULL
        if (avstream->codecpar->sample_rate)
            res->sampleRate = static_cast<std::size_t>(avstream->codecpar->sample_rate);

        return res;
    }
} // namespace lms::audio::ffmpeg