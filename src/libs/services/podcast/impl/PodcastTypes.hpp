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

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <Wt/WDateTime.h>

namespace lms::podcast
{

    struct EnclosureUrl
    {
        std::string url;
        std::size_t length;
        std::string type;
    };

    struct PodcastEpisode
    {
        std::string url;
        std::string title;
        std::string link;
        std::string description;
        Wt::WDateTime pubDate;
        std::string author;
        std::string category;
        std::optional<bool> explicitContent;
        std::string imageUrl;
        std::string ownerEmail;
        std::string guid;
        EnclosureUrl enclosureUrl;
        std::chrono::milliseconds duration{ 0 };
    };

    struct Podcast
    {
        std::string title;
        std::string link;
        std::string description;
        std::string language;
        std::string copyright;
        Wt::WDateTime lastBuildDate;
        // itunes fields
        std::string newUrl;
        std::string author;
        std::string category;
        std::optional<bool> explicitContent;
        std::string imageUrl;
        std::string ownerEmail;
        std::string ownerName;
        std::string subtitle;
        std::string summary;

        std::vector<PodcastEpisode> episodes; // List of episodes in the podcast
    };

} // namespace lms::podcast