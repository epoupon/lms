/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <system_error>

#include "core/Exception.hpp"

namespace lms::metadata
{
    class Exception : public core::LmsException
    {
    public:
        using LmsException::LmsException;
    };

    class IOException : public Exception
    {
    public:
        IOException(std::string_view message, std::error_code err)
            : Exception{ std::string{ message } + ": " + err.message() }
            , _err{ err }
        {
        }

        std::error_code getErrorCode() const { return _err; }

    private:
        std::error_code _err;
    };

    class AudioFileParsingException : public Exception
    {
    public:
        using Exception::Exception;
    };

    class AudioFileNoAudioPropertiesException : public AudioFileParsingException
    {
    public:
        using AudioFileParsingException::AudioFileParsingException;
    };

} // namespace lms::metadata