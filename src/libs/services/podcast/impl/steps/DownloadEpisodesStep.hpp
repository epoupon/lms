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

#include <deque>

#include "database/objects/PodcastEpisodeId.hpp"

#include "RefreshStep.hpp"

namespace lms::podcast
{
    class DownloadEpisodesStep : public RefreshStep
    {
    public:
        DownloadEpisodesStep(RefreshContext& context, OnDoneCallback callback);

    private:
        core::LiteralString getName() const override;
        void run() override;

        void collectEpisodes();

        void processNext();
        void process(db::PodcastEpisodeId episodeId);

        const bool _autoDownloadEpisodes;
        const std::chrono::days _autoDownloadEpisodesMaxAge;

        std::deque<db::PodcastEpisodeId> _episodesToDownload;
    };

} // namespace lms::podcast