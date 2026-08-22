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

#include "audio/IAudioFileInfoParser.hpp"
#include "core/Exception.hpp"
#include "core/IConfig.hpp"
#include "core/Service.hpp"

#include "scanners/audiofile/AudioFileInfoParserSet.hpp"

namespace lms::scanner
{
    namespace
    {
        audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle getParserReadStyle()
        {
            std::string_view readStyle{ core::Service<core::IConfig>::get()->getString("scanner-parser-read-style", "average") };

            if (readStyle == "fast")
                return audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle::Fast;
            if (readStyle == "average")
                return audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle::Average;
            if (readStyle == "accurate")
                return audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle::Accurate;

            throw core::LmsException{ "Invalid value for 'scanner-parser-read-style'" };
        }
    } // namespace

    AudioFileInfoParserSet createAudioFileInfoParserSet()
    {
        AudioFileInfoParserSet parserSet;

        parserSet.taglibParser = audio::createAudioFileInfoParser(audio::AudioFileInfoParserBackend::TagLib);
        parserSet.ffmpegParser = audio::createAudioFileInfoParser(audio::AudioFileInfoParserBackend::FFmpeg);

        // TagLib decides what to scan, ffmpeg is only used as a fallback
        const auto extensions{ parserSet.taglibParser->getSupportedExtensions() };
        parserSet.supportedExtensions.assign(std::cbegin(extensions), std::cend(extensions));

        parserSet.audioPropertiesReadStyle = getParserReadStyle();

        return parserSet;
    }
} // namespace lms::scanner
