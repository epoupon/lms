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

#include <filesystem>
#include <memory>

#include <boost/asio/io_context.hpp>

#include "database/objects/PodcastEpisodeId.hpp"
#include "database/objects/PodcastId.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::podcast
{
    class IPodcastService
    {
    public:
        virtual ~IPodcastService() = default;

        virtual std::filesystem::path getCachePath() const = 0;

        virtual db::PodcastId addPodcast(std::string_view url) = 0;
        virtual bool removePodcast(db::PodcastId podcast) = 0;
        virtual void refreshPodcasts() = 0;

        virtual bool downloadPodcastEpisode(db::PodcastEpisodeId episode) = 0;
        virtual bool deletePodcastEpisode(db::PodcastEpisodeId episode) = 0;
    };

    std::unique_ptr<IPodcastService> createPodcastService(boost::asio::io_context& ioContext, db::IDb& db, const std::filesystem::path& cachePath);
} // namespace lms::podcast
