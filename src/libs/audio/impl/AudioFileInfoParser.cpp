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

#include <memory>

#include "audio/IAudioFileInfoParser.hpp"

#include "ffmpeg/AudioFileInfoParser.hpp"
#include "taglib/AudioFileInfoParser.hpp"

namespace lms::audio
{
    std::unique_ptr<IAudioFileInfoParser> createAudioFileInfoParser(AudioFileInfoParserBackend backend)
    {
        switch (backend)
        {
        case AudioFileInfoParserBackend::TagLib:
            return std::make_unique<taglib::AudioFileInfoParser>();
        case AudioFileInfoParserBackend::FFmpeg:
            return std::make_unique<ffmpeg::AudioFileInfoParser>();
        }

        return nullptr;
    }

    core::LiteralString audioFileInfoParserBackendToString(AudioFileInfoParserBackend backend)
    {
        switch (backend)
        {
        case AudioFileInfoParserBackend::TagLib:
            return "taglib";
        case AudioFileInfoParserBackend::FFmpeg:
            return "ffmpeg";
        }

        return "Unknown";
    }
} // namespace lms::audio