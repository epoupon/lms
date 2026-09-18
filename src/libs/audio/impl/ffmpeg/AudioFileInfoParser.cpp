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

#include "AudioFileInfoParser.hpp"

#include <memory>

#include "AudioFileInfo.hpp"
#include "Utils.hpp"

namespace lms::audio::ffmpeg
{
    std::unique_ptr<IAudioFileInfo> AudioFileInfoParser::parse(const std::filesystem::path& p, const AudioFileInfoParseOptions& parseOptions) const
    {
        return std::make_unique<AudioFileInfo>(p, parseOptions);
    }

    std::span<const std::filesystem::path> AudioFileInfoParser::getSupportedExtensions() const
    {
        return utils::getSupportedDemuxerExtensions();
    }
} // namespace lms::audio::ffmpeg