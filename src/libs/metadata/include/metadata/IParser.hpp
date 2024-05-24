/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <filesystem>
#include <memory>
#include <span>

#include "metadata/Types.hpp"

namespace lms::metadata
{
    class IParser
    {
    public:
        virtual ~IParser() = default;

        virtual std::unique_ptr<Track> parse(const std::filesystem::path& p, bool debug = false) = 0;

        virtual void setUserExtraTags(std::span<const std::string> extraTags) = 0;
        virtual void setArtistTagDelimiters(std::span<const std::string> delimiters) = 0;
        virtual void setDefaultTagDelimiters(std::span<const std::string> delimiters) = 0;
    };

    enum class ParserBackend
    {
        TagLib,
        AvFormat,
    };

    enum class ParserReadStyle
    {
        Fast,
        Average,
        Accurate,
    };
    std::unique_ptr<IParser> createParser(ParserBackend parserBackend, ParserReadStyle parserReadStyle);
} // namespace lms::metadata
