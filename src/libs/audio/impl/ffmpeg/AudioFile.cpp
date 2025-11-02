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

#include <array>
#include <unordered_map>

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "audio/AudioTypes.hpp"
#include "audio/IAudioFileInfo.hpp"

namespace lms::audio::ffmpeg
{
    namespace
    {
        std::string averror_to_string(int error)
        {
            std::array<char, 128> buf = { 0 };

            if (::av_strerror(error, buf.data(), buf.size()) == 0)
                return buf.data();

            return "Unknown error";
        }

        class AudioFileException : public AudioFileParsingException
        {
        public:
            AudioFileException(int avError)
                : AudioFileParsingException{ averror_to_string(avError) }
            {
            }
        };

        void getMetaDataFromDictionnary(AVDictionary* dictionnary, AudioFile::MetadataMap& res)
        {
            if (!dictionnary)
                return;

            AVDictionaryEntry* tag = NULL;
            while ((tag = ::av_dict_get(dictionnary, "", tag, AV_DICT_IGNORE_SUFFIX)))
            {
                res[core::stringUtils::stringToUpper(tag->key)] = tag->value;
            }
        }

        std::optional<ContainerType> avdemuxerToContainerType(std::string_view name)
        {
            if (name == "aiff")
                return ContainerType::AIFF;
            if (name == "ape")
                return ContainerType::APE;
            if (name.starts_with("asf"))
                return ContainerType::ASF;
            if (name == "dsf")
                return ContainerType::DSF;
            if (name == "flac")
                return ContainerType::FLAC;
            if (name.find("mp4") != std::string_view::npos)
                return ContainerType::MP4;
            if (name.starts_with("mpc"))
                return ContainerType::MPC;
            if (name == "mp3")
                return ContainerType::MPEG;
            if (name == "ogg")
                return ContainerType::Ogg;
            if (name == "shn")
                return ContainerType::Shorten;
            if (name == "tta")
                return ContainerType::TrueAudio;
            if (name == "wav")
                return ContainerType::WAV;
            if (name == "wv")
                return ContainerType::WavPack;

            return std::nullopt;
        }

        std::optional<CodecType> avcodecToCodecType(AVCodecID codec)
        {
            switch (codec)
            {
            case AV_CODEC_ID_MP3:
                return CodecType::MP3;
            case AV_CODEC_ID_AAC:
                return CodecType::AAC;
            case AV_CODEC_ID_AC3:
                return CodecType::AC3;
            case AV_CODEC_ID_VORBIS:
                return CodecType::Vorbis;
            case AV_CODEC_ID_WMAV1:
                return CodecType::WMA1;
            case AV_CODEC_ID_WMAV2:
                return CodecType::WMA2;
            case AV_CODEC_ID_WMAPRO:
                return CodecType::WMA9Pro;
            case AV_CODEC_ID_WMALOSSLESS:
                return CodecType::WMA9Lossless;
            case AV_CODEC_ID_FLAC:
                return CodecType::FLAC;
            case AV_CODEC_ID_ALAC:
                return CodecType::ALAC;
            case AV_CODEC_ID_WAVPACK:
                return CodecType::WavPack;
            case AV_CODEC_ID_MUSEPACK7:
                return CodecType::MPC7;
            case AV_CODEC_ID_MUSEPACK8:
                return CodecType::MPC8;
            case AV_CODEC_ID_APE:
                return CodecType::APE;
            case AV_CODEC_ID_EAC3:
                return CodecType::EAC3;
            case AV_CODEC_ID_MP4ALS:
                return CodecType::MP4ALS;
            case AV_CODEC_ID_OPUS:
                return CodecType::Opus;
            case AV_CODEC_ID_SHORTEN:
                return CodecType::Shorten;
            case AV_CODEC_ID_DSD_LSBF:
            case AV_CODEC_ID_DSD_LSBF_PLANAR:
            case AV_CODEC_ID_DSD_MSBF:
            case AV_CODEC_ID_DSD_MSBF_PLANAR:
                return CodecType::DSD;

            default:
                return std::nullopt;
            }
        }
    } // namespace

    AudioFile::AudioFile(const std::filesystem::path& p)
        : _p{ p }
    {
        int error{ avformat_open_input(&_context, _p.c_str(), nullptr, nullptr) };
        if (error < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot open " << _p << ": " << averror_to_string(error));
            throw AudioFileException{ error };
        }

        error = avformat_find_stream_info(_context, nullptr);
        if (error < 0)
        {
            LMS_LOG(AUDIO, ERROR, "Cannot find stream information on " << _p << ": " << averror_to_string(error));
            avformat_close_input(&_context);
            throw AudioFileException{ error };
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

        info.bitrate = _context->bit_rate;
        info.duration = std::chrono::milliseconds{ _context->duration == AV_NOPTS_VALUE ? 0 : _context->duration / AV_TIME_BASE * 1'000 };

        return info;
    }

    AudioFile::MetadataMap AudioFile::getMetaData() const
    {
        MetadataMap res;

        getMetaDataFromDictionnary(_context->metadata, res);

        // HACK for OGG files
        // If we did not find tags, search metadata in streams
        if (res.empty())
        {
            for (std::size_t i{}; i < _context->nb_streams; ++i)
            {
                getMetaDataFromDictionnary(_context->streams[i]->metadata, res);

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
        for (std::size_t i = 0; i < _context->nb_streams; ++i)
        {
            if (_context->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
                return true;
        }

        return false;
    }

    void AudioFile::visitAttachedPictures(std::function<void(const Picture&, const MetadataMap&)> func) const
    {
        static const std::unordered_map<int, std::string> codecMimeMap{
            { AV_CODEC_ID_BMP, "image/bmp" },
            { AV_CODEC_ID_GIF, "image/gif" },
            { AV_CODEC_ID_MJPEG, "image/jpeg" },
            { AV_CODEC_ID_PNG, "image/png" },
            { AV_CODEC_ID_PPM, "image/x-portable-pixmap" },
        };

        for (std::size_t i = 0; i < _context->nb_streams; ++i)
        {
            AVStream* avstream = _context->streams[i];

            // Skip attached pics
            if (!(avstream->disposition & AV_DISPOSITION_ATTACHED_PIC))
                continue;

            if (avstream->codecpar == nullptr)
            {
                LMS_LOG(AUDIO, ERROR, "Skipping stream " << i << " since no codecpar is set");
                continue;
            }

            MetadataMap metadata;
            getMetaDataFromDictionnary(avstream->metadata, metadata);

            Picture picture;

            auto itMime = codecMimeMap.find(avstream->codecpar->codec_id);
            if (itMime != codecMimeMap.end())
            {
                picture.mimeType = itMime->second;
            }
            else
            {
                picture.mimeType = "application/octet-stream";
                LMS_LOG(AUDIO, ERROR, "CODEC ID " << avstream->codecpar->codec_id << " not handled in mime type conversion");
            }

            const AVPacket& pkt{ avstream->attached_pic };

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
            LMS_LOG(AUDIO, ERROR, "Skipping stream " << streamIndex << " since no codecpar is set");
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