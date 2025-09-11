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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include <boost/asio/steady_timer.hpp>

#include "database/objects/PodcastId.hpp"
#include "services/podcast/IPodcastService.hpp"

#include "Executor.hpp"
#include "RefreshContext.hpp"

namespace lms::core::http
{
    class IClient;
}

namespace lms::podcast
{
    class RefreshStep;

    class PodcastService : public IPodcastService
    {
    public:
        PodcastService(boost::asio::io_context& ioContext, db::IDb& db, const std::filesystem::path& cachePath);
        ~PodcastService() override;

        PodcastService(const PodcastService&) = delete;
        PodcastService& operator=(const PodcastService&) = delete;

    private:
        std::filesystem::path getCachePath() const override;

        db::PodcastId addPodcast(std::string_view url) override;
        bool removePodcast(db::PodcastId podcast) override;
        void refreshPodcasts() override;

        bool downloadPodcastEpisode(db::PodcastEpisodeId episode) override;
        bool deletePodcastEpisode(db::PodcastEpisodeId episode) override;

        void abortCurrentRefresh(std::unique_lock<std::mutex>& lock);
        void allowRefresh();
        void scheduleRefresh(std::chrono::seconds fromNow = std::chrono::seconds::zero());
        void startRefresh();
        void onRefreshDone();

        void setupSteps();
        void onCurrentStepDone(bool success);
        void runNextStep();
        void runStep(std::size_t stepIndex);

        Executor _executor;
        boost::asio::steady_timer _refreshTimer;
        std::unique_ptr<core::http::IClient> _httpClient;
        RefreshContext _refreshContext;

        const std::chrono::hours _refreshPeriod;

        std::mutex _controlMutex;
        std::condition_variable _controlCv;
        std::atomic<bool> _refreshInProgress;

        std::atomic<bool> _abortRequested;
        std::vector<std::unique_ptr<RefreshStep>> _refreshSteps;
        std::size_t _refreshStepIndex;
    };
} // namespace lms::podcast
