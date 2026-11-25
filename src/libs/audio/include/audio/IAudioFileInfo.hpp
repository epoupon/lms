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

#include <optional>

namespace lms::audio
{
    struct AudioProperties;
    class IImageReader;
    class ITagReader;

    enum class AudioPropertiesError
    {
        NoAudioStream,
        UnsupportedFormat,
        UnknownDuration,
    };

    class IAudioFileInfo
    {
    public:
        virtual ~IAudioFileInfo() = default;

        virtual const AudioProperties* getAudioProperties() const = 0;                   // null if the audio cannot be handled, see getAudioPropertiesError()
        virtual std::optional<AudioPropertiesError> getAudioPropertiesError() const = 0; // set if and only if getAudioProperties() is null
        virtual const IImageReader* getImageReader() const = 0;
        virtual const ITagReader* getTagReader() const = 0;
    };
} // namespace lms::audio
