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

#include <chrono>
#include <filesystem>
#include <optional>
#include <variant>

#include "core/ILogger.hpp"

#include "audio/AudioProperties.hpp"
#include "audio/IAudioFileInfo.hpp"

#include "AudioFile.hpp"
#include "ImageReader.hpp"
#include "TagReader.hpp"

namespace lms::audio::ffmpeg
{
    namespace
    {
        std::variant<AudioProperties, AudioPropertiesError> computeAudioProperties(const AudioFile& audioFile)
        {
            const auto containerInfo{ audioFile.getContainerInfo() };
            const auto bestStreamInfo{ audioFile.getBestStreamInfo() };
            if (!bestStreamInfo)
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot find best audio stream in " << audioFile.getPath());
                return AudioPropertiesError::NoAudioStream;
            }

            if (!containerInfo.container)
            {
                LMS_LOG(AUDIO, DEBUG, "Unhandled container '" << containerInfo.containerName << "' in " << audioFile.getPath());
                return AudioPropertiesError::UnsupportedFormat;
            }

            if (!bestStreamInfo->codec)
            {
                LMS_LOG(AUDIO, DEBUG, "Unhandled codec '" << bestStreamInfo->codecName << "' in " << audioFile.getPath());
                return AudioPropertiesError::UnsupportedFormat;
            }

            if (!bestStreamInfo->channelCount || *bestStreamInfo->channelCount == 0)
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot determine channel count in " << audioFile.getPath());
                return AudioPropertiesError::NoAudioStream;
            }

            if (!bestStreamInfo->sampleRate || *bestStreamInfo->sampleRate == 0)
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot determine sample rate in " << audioFile.getPath());
                return AudioPropertiesError::NoAudioStream;
            }

            if (containerInfo.duration <= std::chrono::milliseconds::zero())
            {
                LMS_LOG(AUDIO, DEBUG, "Cannot determine duration in " << audioFile.getPath());
                return AudioPropertiesError::UnknownDuration;
            }

            AudioProperties audioProperties;

            audioProperties.container = *containerInfo.container;
            audioProperties.duration = containerInfo.duration;
            audioProperties.codec = *bestStreamInfo->codec;

            if (bestStreamInfo->bitrate)
                audioProperties.bitrate = static_cast<unsigned>(*bestStreamInfo->bitrate);
            else if (containerInfo.bitrate)
                audioProperties.bitrate = static_cast<unsigned>(*containerInfo.bitrate);

            audioProperties.channelCount = *bestStreamInfo->channelCount;
            audioProperties.sampleRate = *bestStreamInfo->sampleRate;
            audioProperties.bitsPerSample = bestStreamInfo->bitsPerSample;

            return audioProperties;
        }
    } // namespace

    AudioFileInfo::AudioFileInfo(const std::filesystem::path& filePath, const AudioFileInfoParseOptions& parseOptions)
        : _audioFile{ std::make_unique<AudioFile>(filePath) }
        , _audioProperties{ computeAudioProperties(*_audioFile) }
    {
        if (parseOptions.readTags)
            _tagReader = std::make_unique<TagReader>(*_audioFile, parseOptions.enableExtraDebugLogs);

        if (parseOptions.readImages)
            _imageReader = std::make_unique<ImageReader>(*_audioFile);
    }

    AudioFileInfo::~AudioFileInfo() = default;

    const AudioProperties* AudioFileInfo::getAudioProperties() const
    {
        return std::get_if<AudioProperties>(&_audioProperties);
    }

    std::optional<AudioPropertiesError> AudioFileInfo::getAudioPropertiesError() const
    {
        if (const AudioPropertiesError * error{ std::get_if<AudioPropertiesError>(&_audioProperties) })
            return *error;

        return std::nullopt;
    }

    const IImageReader* AudioFileInfo::getImageReader() const
    {
        return _imageReader.get();
    }

    const ITagReader* AudioFileInfo::getTagReader() const
    {
        return _tagReader.get();
    }
} // namespace lms::audio::ffmpeg