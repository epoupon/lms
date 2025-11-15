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

#include <set>
#include <string>

#include "audio/IAudioFileInfo.hpp"
#include "audio/ITagReader.hpp"

#include "types/TrackMetadata.hpp"

namespace lms::scanner
{
    class TrackMetadataParser
    {
    public:
        struct SortByLengthDesc
        {
            bool operator()(const std::string& a, const std::string& b) const
            {
                if (a.length() != b.length())
                    return a.length() > b.length();
                return a < b; // Break ties using lexicographical order
            }
        };

        using WhiteList = std::set<std::string, SortByLengthDesc>;
        struct Parameters
        {
            std::vector<std::string> artistTagDelimiters;
            WhiteList artistsToNotSplit;
            std::vector<std::string> defaultTagDelimiters;
            std::vector<std::string> userExtraTags;
        };

        TrackMetadataParser(const Parameters& params = {});
        ~TrackMetadataParser();
        TrackMetadataParser(const TrackMetadataParser&) = delete;
        TrackMetadataParser& operator=(const TrackMetadataParser&) = delete;

        Track parseTrackMetaData(const audio::ITagReader& reader) const;

    private:
        void processTags(const audio::ITagReader& reader, Track& track) const;

        std::optional<Medium> getMedium(const audio::ITagReader& tagReader) const;
        std::optional<Release> getRelease(const audio::ITagReader& tagReader) const;

        const Parameters _params;
    };
} // namespace lms::scanner
