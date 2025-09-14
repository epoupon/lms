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

#include "PodcastService.hpp"

#include <filesystem>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "steps/CheckForMissingFilesStep.hpp"
#include "steps/ClearTmpDirectoryStep.hpp"
#include "steps/DownloadEpisodeArtworksStep.hpp"
#include "steps/DownloadEpisodesStep.hpp"
#include "steps/DownloadPodcastArtworksStep.hpp"
#include "steps/RefreshPodcastsStep.hpp"
#include "steps/RemoveEpisodesStep.hpp"
#include "steps/RemovePodcastsStep.hpp"

#include "Exception.hpp"

namespace lms::podcast
{
    std::unique_ptr<IPodcastService> createPodcastService(boost::asio::io_context& ioContext, db::IDb& db, const std::filesystem::path& cachePath)
    {
        return std::make_unique<PodcastService>(ioContext, db, cachePath);
    }

    PodcastService::PodcastService(boost::asio::io_context& ioContext, db::IDb& db, const std::filesystem::path& cachePath)
        : _executor{ ioContext }
        , _refreshTimer(ioContext)
        , _httpClient{ core::http::createClient(ioContext, "") }
        , _refreshContext{ _executor, db, *_httpClient, cachePath }
        , _refreshPeriod{ core::Service<core::IConfig>::get()->getULong("podcast-refresh-period-hours", 2) }
        , _refreshInProgress{ false }
        , _abortRequested{ false }
        , _refreshStepIndex{ 0 }
    {
        if (_refreshPeriod.count() < 1)
        {
            LMS_LOG(PODCAST, ERROR, "Podcast refresh period must be at least 1 hour");
            throw Exception{ "Podcast refresh period must be at least 1 hour" };
        }

        setupSteps();

        std::filesystem::create_directories(_refreshContext.cachePath);
        std::filesystem::create_directories(_refreshContext.tmpCachePath);

        LMS_LOG(PODCAST, INFO, "Starting service...");
        scheduleRefresh(std::chrono::seconds{ 1 });
        LMS_LOG(PODCAST, INFO, "Service started!");
    }

    PodcastService::~PodcastService()
    {
        std::unique_lock lock{ _controlMutex };

        abortCurrentRefresh(lock);
        LMS_LOG(PODCAST, INFO, "Service stopped!");
    }

    std::filesystem::path PodcastService::getCachePath() const
    {
        return _refreshContext.cachePath;
    }

    db::PodcastId PodcastService::addPodcast(std::string_view url)
    {
        std::unique_lock lock{ _controlMutex };

        abortCurrentRefresh(lock);

        db::PodcastId podcastId;
        {
            db::Session& session{ _refreshContext.db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            db::Podcast::pointer podcast{ db::Podcast::find(session, url) };
            if (!podcast)
                podcast = session.create<db::Podcast>(url);

            podcastId = podcast->getId();
        }

        allowRefresh();
        scheduleRefresh();

        return podcastId;
    }

    bool PodcastService::removePodcast(db::PodcastId podcastId)
    {
        bool res{};
        std::unique_lock lock{ _controlMutex };

        abortCurrentRefresh(lock);

        {
            db::Session& session{ _refreshContext.db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            db::Podcast::pointer podcast{ db::Podcast::find(session, podcastId) };
            if (podcast)
            {
                podcast.modify()->setDeleteRequested(true);
                res = true;
            }
        }

        allowRefresh();
        scheduleRefresh();

        return res;
    }

    void PodcastService::refreshPodcasts()
    {
        std::unique_lock lock{ _controlMutex };

        abortCurrentRefresh(lock);
        allowRefresh();
        scheduleRefresh();
    }

    bool PodcastService::downloadPodcastEpisode(db::PodcastEpisodeId episodeId)
    {
        bool res{};
        std::unique_lock lock{ _controlMutex };

        abortCurrentRefresh(lock);

        {
            db::Session& session{ _refreshContext.db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            db::PodcastEpisode::pointer episode{ db::PodcastEpisode::find(session, episodeId) };
            if (episode)
            {
                episode.modify()->setManualDownloadState(db::PodcastEpisode::ManualDownloadState::DownloadRequested);
                res = true;
            }
        }

        allowRefresh();
        scheduleRefresh();

        return res;
    }

    bool PodcastService::deletePodcastEpisode(db::PodcastEpisodeId episodeId)
    {
        bool res{};
        std::unique_lock lock{ _controlMutex };

        abortCurrentRefresh(lock);

        {
            db::Session& session{ _refreshContext.db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            db::PodcastEpisode::pointer episode{ db::PodcastEpisode::find(session, episodeId) };
            if (episode)
            {
                episode.modify()->setManualDownloadState(db::PodcastEpisode::ManualDownloadState::DeleteRequested);
                res = true;
            }
        }

        allowRefresh();
        scheduleRefresh();

        return res;
    }

    bool PodcastService::hasPodcasts() const
    {
        db::Session& session{ _refreshContext.db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return db::Podcast::getCount(session) > 0;
    }

    void PodcastService::abortCurrentRefresh(std::unique_lock<std::mutex>& lock)
    {
        LMS_LOG(PODCAST, DEBUG, "Aborting current refresh...");

        _abortRequested = true;
        for (auto& step : _refreshSteps)
            step->requestAbort(true);

        _httpClient->abortAllRequests();
        _refreshTimer.cancel();

        _controlCv.wait(lock, [this] {
            return !_refreshInProgress;
        });

        LMS_LOG(PODCAST, DEBUG, "Current refresh aborted!");
    }

    void PodcastService::allowRefresh()
    {
        assert(!_refreshInProgress);
        assert(_abortRequested);

        _abortRequested = false;
        for (auto& step : _refreshSteps)
            step->requestAbort(false);
    }

    void PodcastService::scheduleRefresh(std::chrono::seconds fromNow)
    {
        if (!hasPodcasts())
        {
            LMS_LOG(PODCAST, DEBUG, "No podcast: not scheduling refresh");
            return;
        }

        LMS_LOG(PODCAST, DEBUG, "Scheduled podcast refresh in " << fromNow.count() << " seconds...");

        _refreshTimer.expires_after(fromNow);
        _refreshTimer.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
                return;

            if (ec)
                throw Exception{ "Steady timer failure: " + std::string{ ec.message() } };

            _executor.post([this] { startRefresh(); });
        });
    }

    void PodcastService::startRefresh()
    {
        LMS_LOG(PODCAST, DEBUG, "Starting  podcast refresh");

        _refreshInProgress = true;
        _refreshStepIndex = 0;
        runStep(_refreshStepIndex);
    }

    void PodcastService::setupSteps()
    {
        auto onDoneCallback{ [this](bool success) {
            onCurrentStepDone(success);
        } };

        _refreshSteps.clear();

        // order is important, each step is done only when the previous one is done
        _refreshSteps.emplace_back(std::make_unique<ClearTmpDirectoryStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<CheckForMissingFilesStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<RemovePodcastsStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<RefreshPodcastsStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<RemoveEpisodesStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<DownloadPodcastArtworksStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<DownloadEpisodeArtworksStep>(_refreshContext, onDoneCallback));
        _refreshSteps.emplace_back(std::make_unique<DownloadEpisodesStep>(_refreshContext, onDoneCallback));
    }

    void PodcastService::onCurrentStepDone(bool success)
    {
        LMS_LOG(PODCAST, DEBUG, "Step '" << _refreshSteps[_refreshStepIndex]->getName() << "' done: " << (success ? "success" : _abortRequested ? "aborted" :
                                                                                                                                                  "failure"));

        if (success && !_abortRequested)
            runNextStep();
        else
            onRefreshDone();
    }

    void PodcastService::runNextStep()
    {
        if (++_refreshStepIndex < _refreshSteps.size())
            runStep(_refreshStepIndex);
        else
            onRefreshDone();
    }

    void PodcastService::runStep(std::size_t stepIndex)
    {
        _refreshContext.executor.post([stepIndex, this] {
            assert(stepIndex < _refreshSteps.size());
            RefreshStep& step{ *_refreshSteps[stepIndex] };

            LMS_LOG(PODCAST, DEBUG, "Running step '" << step.getName() << "'");
            {
                LMS_SCOPED_TRACE_OVERVIEW("Podcast", step.getName());
                step.run();
            }
        });
    }

    void PodcastService::onRefreshDone()
    {
        LMS_LOG(PODCAST, DEBUG, "Refresh done" << (_abortRequested ? " (aborted)" : ""));

        const bool rescheduleRefresh{ !_abortRequested };

        _refreshInProgress = false;
        _controlCv.notify_all();

        if (rescheduleRefresh)
            scheduleRefresh(_refreshPeriod);
    }
} // namespace lms::podcast
