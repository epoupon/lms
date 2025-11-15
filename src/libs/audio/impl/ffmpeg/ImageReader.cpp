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

#include "ImageReader.hpp"

#include <algorithm>

#include "core/String.hpp"

#include "AudioFile.hpp"

namespace lms::audio::ffmpeg
{
    ImageReader::ImageReader(const AudioFile& audioFile)
        : _audioFile{ audioFile }
    {
    }

    ImageReader::~ImageReader() = default;

    void ImageReader::visitImages(const ImageVisitor& visitor) const
    {
        auto metaDataHasKeyword{ [](const AudioFile::MetadataMap& metadata, std::string_view keyword) {
            return std::any_of(std::cbegin(metadata), std::cend(metadata), [&](const auto& keyValue) { return core::stringUtils::stringCaseInsensitiveContains(keyValue.second, keyword); });
        } };

        _audioFile.visitAttachedPictures([&](const Picture& picture, const AudioFile::MetadataMap& metaData) {
            Image image;
            image.data = picture.data;
            image.mimeType = picture.mimeType;
            if (metaDataHasKeyword(metaData, "front"))
                image.type = Image::Type::FrontCover;
            else if (metaDataHasKeyword(metaData, "back"))
                image.type = Image::Type::BackCover;

            visitor(image);
        });
    }

} // namespace lms::audio::ffmpeg
