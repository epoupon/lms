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

#include "Utils.hpp"
#include "core/String.hpp"

#include <array>

extern "C"
{
#include <libavutil/error.h>
#include <libavutil/log.h>
}

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"

namespace lms::audio::ffmpeg::utils
{
    namespace
    {
        core::LiteralString avLogLevelToStr(int level)
        {
            switch (level)
            {
            case AV_LOG_TRACE:
                return "trace";
            case AV_LOG_DEBUG:
                return "debug";
            case AV_LOG_VERBOSE:
                return "verbose";
            case AV_LOG_INFO:
                return "info";
            case AV_LOG_WARNING:
                return "warning";
            case AV_LOG_ERROR:
                return "error";
            case AV_LOG_FATAL:
                return "fatal";
            case AV_LOG_PANIC:
                return "panic";
            default:
                return "unknown";
            }
        }

        void avLogCallback(void*, int level, const char* fmt, va_list vl)
        {
            if (!core::Service<core::logging::ILogger>::get() || core::Service<core::logging::ILogger>::get()->isSeverityActive(core::logging::Severity::DEBUG))
                return;

            if (level > AV_LOG_WARNING)
                return;

            std::array<char, 256> buffer{ 0 };
            if (std::vsnprintf(buffer.data(), buffer.size(), fmt, vl) > 0)
            {
                std::string_view str{ buffer.data() };
                str = core::stringUtils::stringTrimEnd(str, " \t\r\n");

                // TODO translate levels?
                LMS_LOG(AUDIO, DEBUG, "[FFmpeg] [" << avLogLevelToStr(level) << "] " << str);
            }
        }

        class AvInitializer
        {
        public:
            AvInitializer()
            {
                ::av_log_set_callback(avLogCallback);
            }
        };
    } // namespace

    std::string averrorToString(int error)
    {
        std::array<char, 128> buf{ 0 };

        if (::av_strerror(error, buf.data(), buf.size()) == 0)
            return buf.data();

        return "Unknown error";
    }

    std::span<const std::filesystem::path> getSupportedExtensions()
    {
        // TODO: list demuxers to retrieve supported formats
        static const std::array<std::filesystem::path, 19> fileExtensions{
            ".aac",
            ".alac",
            ".aif",
            ".aifc",
            ".aiff",
            ".ape",
            ".dsf",
            ".flac",
            ".m4a",
            ".m4b",
            ".mp3",
            ".mpc",
            ".oga",
            ".ogg",
            ".opus",
            ".shn",
            ".wav",
            ".wma",
            ".wv",
        };
        return fileExtensions;
    }

    void init()
    {
        static AvInitializer init;
    }
} // namespace lms::audio::ffmpeg::utils