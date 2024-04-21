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

#include "metadata/IParser.hpp"
#include "ITagReader.hpp"

namespace lms::metadata
{
    class Parser : public IParser
    {
    public:
        Parser(ParserBackend parserBackend = ParserBackend::TagLib, ParserReadStyle readStyle = ParserReadStyle::Average);

        std::unique_ptr<Track> parse(const std::filesystem::path& p, bool debug = false) override;
        std::unique_ptr<Track> parse(const ITagReader& reader);

    private:
        void setUserExtraTags(std::span<const std::string> extraTags) override { _userExtraTags.assign(std::cbegin(extraTags), std::cend(extraTags)); }
        void setArtistTagDelimiters(std::span<const std::string> delimiters) override { _artistTagDelimiters.assign(std::cbegin(delimiters), std::cend(delimiters)); }
        void setDefaultTagDelimiters(std::span<const std::string> delimiters) override { _defaultTagDelimiters.assign(std::cbegin(delimiters), std::cend(delimiters)); }

        void processTags(const ITagReader& reader, Track& track);

        std::optional<Medium> getMedium(const ITagReader& tagReader);
        std::optional<Release> getRelease(const ITagReader& tagReader);

        const ParserBackend _parserBackend;
        const ParserReadStyle _readStyle;

        std::vector<std::string> _userExtraTags;
        std::vector<std::string> _artistTagDelimiters;
        std::vector<std::string> _defaultTagDelimiters;
    };
} // namespace lms::metadata

