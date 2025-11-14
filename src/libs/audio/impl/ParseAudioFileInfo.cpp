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

#include <memory>

#include "audio/IAudioFileInfo.hpp"

#include "ffmpeg/AudioFileInfo.hpp"
#include "ffmpeg/Utils.hpp"
#include "taglib/AudioFileInfo.hpp"
#include "taglib/Utils.hpp"

namespace lms::audio
{
    std::unique_ptr<IAudioFileInfo> parseAudioFile(const std::filesystem::path& p, const ParserOptions& parserOptions)
    {
        switch (parserOptions.parser)
        {
        case ParserOptions::Parser::TagLib:
            return std::make_unique<taglib::AudioFileInfo>(p, parserOptions.readStyle, parserOptions.enableExtraDebugLogs);
        case ParserOptions::Parser::FFmpeg:
            return std::make_unique<ffmpeg::AudioFileInfo>(p, parserOptions.enableExtraDebugLogs);
        }

        return {};
    }

    std::span<const std::filesystem::path> getSupportedExtensions(ParserOptions::Parser parser)
    {
        switch (parser)
        {
        case ParserOptions::Parser::TagLib:
            return taglib::utils::getSupportedExtensions();
        case ParserOptions::Parser::FFmpeg:
            return ffmpeg::utils::getSupportedExtensions();
        }

        return {};
    }

} // namespace lms::audio