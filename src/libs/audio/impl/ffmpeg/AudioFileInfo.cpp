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

#include "AudioFileInfo.hpp"

#include "audio/AudioTypes.hpp"
#include "audio/IAudioFileInfo.hpp"

#include "AudioFile.hpp"
#include "ImageReader.hpp"
#include "TagReader.hpp"

namespace lms::audio::ffmpeg
{
    namespace
    {
        AudioProperties computeAudioProperties(const AudioFile& audioFile)
        {
            AudioProperties audioProperties;

            const auto containerInfo{ audioFile.getContainerInfo() };
            const auto bestStreamInfo{ audioFile.getBestStreamInfo() };
            if (!bestStreamInfo)
                throw AudioFileParsingException{ audioFile.getPath(), "Cannot find best audio stream" };

            if (!containerInfo.container)
                throw AudioFileParsingException{ audioFile.getPath(), "Unhandled container type '" + containerInfo.containerName + "'" };

            audioProperties.container = *containerInfo.container;
            audioProperties.duration = containerInfo.duration;
            audioProperties.codec = bestStreamInfo->codec;
            audioProperties.bitrate = bestStreamInfo->bitrate;
            audioProperties.bitsPerSample = bestStreamInfo->bitsPerSample;
            audioProperties.channelCount = bestStreamInfo->channelCount;
            audioProperties.sampleRate = bestStreamInfo->sampleRate;

            return audioProperties;
        }
    } // namespace

    AudioFileInfo::AudioFileInfo(const std::filesystem::path& filePath, bool enableExtraDebugLogs)
        : _audioFile{ std::make_unique<AudioFile>(filePath) }
        , _audioProperties{ std::make_unique<AudioProperties>(computeAudioProperties(*_audioFile)) }
        , _tagReader{ std::make_unique<TagReader>(*_audioFile, enableExtraDebugLogs) }
        , _imageReader{ std::make_unique<ImageReader>(*_audioFile) }
    {
    }

    AudioFileInfo::~AudioFileInfo() = default;

    const AudioProperties& AudioFileInfo::getAudioProperties() const
    {
        return *_audioProperties;
    }

    const IImageReader& AudioFileInfo::getImageReader() const
    {
        return *_imageReader;
    }

    const ITagReader& AudioFileInfo::getTagReader() const
    {
        return *_tagReader;
    }

} // namespace lms::audio::ffmpeg
