/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <taglib/tfile.h>

#include "audio/AudioTypes.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IImageReader.hpp"
#include "audio/ITagReader.hpp"

#include "ImageReader.hpp"
#include "TagReader.hpp"

namespace lms::audio::taglib
{
    class AudioFileInfo final : public IAudioFileInfo
    {
    public:
        AudioFileInfo(const std::filesystem::path& filePath, ParserOptions::AudioPropertiesReadStyle readStyle, bool enableExtraDebugLogs);

    private:
        const AudioProperties& getAudioProperties() const override;
        const IImageReader& getImageReader() const override;
        const ITagReader& getTagReader() const override;

        const std::filesystem::path _filePath;
        std::unique_ptr<::TagLib::File> _file;
        const AudioProperties _audioProperties;
        const TagReader _tagReader;
        const ImageReader _imageReader;
    };
} // namespace lms::audio::taglib
