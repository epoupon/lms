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

#include "DownloadEpisodeArtworksStep.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "core/ILogger.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "Executor.hpp"
#include "Utils.hpp"

namespace lms::podcast
{
    namespace
    {
        void createEpisodeArtwork(db::Session& session, db::PodcastEpisodeId episodeId, const std::filesystem::path& filePath, std::string_view contentType)
        {
            auto transaction{ session.createWriteTransaction() };

            db::PodcastEpisode::pointer episode{ db::PodcastEpisode::find(session, episodeId) };
            if (!episode)
                return;

            if (db::Artwork::pointer artwork{ utils::createArtworkFromImage(session, filePath, contentType) })
                episode.modify()->setArtwork(artwork);
        }
    } // namespace

    core::LiteralString DownloadEpisodeArtworksStep::getName() const
    {
        return "Download episode artworks";
    }

    void DownloadEpisodeArtworksStep::run()
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        _episodeArtworksToDownload.clear();
        db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode::pointer& episode) {
            if (episode->getImageUrl().empty())
                return;

            if (episode->getArtworkId().isValid())
                return;

            _episodeArtworksToDownload.push_back(episode->getId());
        });

        processNext();
    }

    void DownloadEpisodeArtworksStep::processNext()
    {
        if (abortRequested())
        {
            onAbort();
            return;
        }

        getExecutor().post([this] {
            if (_episodeArtworksToDownload.empty())
            {
                onDone();
                return;
            }

            const db::PodcastEpisodeId podcastEpisodeId{ _episodeArtworksToDownload.front() };
            _episodeArtworksToDownload.pop_front();
            process(podcastEpisodeId);
        });
    }

    void DownloadEpisodeArtworksStep::process(db::PodcastEpisodeId episodeId)
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const auto episode{ db::PodcastEpisode::find(getDb().getTLSSession(), episodeId) };
        if (!episode)
        {
            LMS_LOG(PODCAST, DEBUG, "Cannot find episode: removed?");
            processNext();
            return;
        }

        const std::string url{ episode->getImageUrl() };
        const std::filesystem::path finalFilePath{ getCachePath() / utils::generateRandomFileName() };

        core::http::ClientGETRequestParameters params;
        params.relativeUrl = episode->getImageUrl();
        params.onFailureFunc = [this, episode] {
            LMS_LOG(PODCAST, ERROR, "Failed to download episode image from '" << episode->getImageUrl() << "'");
            processNext();
        };
        params.onSuccessFunc = [=, this](const Wt::Http::Message& msg) {
            const std::string body{ msg.body() }; // API enforces a copy here

            std::ofstream file{ finalFilePath, std::ios::binary | std::ios::trunc };
            if (!file)
            {
                std::error_code ec{ errno, std::generic_category() };
                LMS_LOG(PODCAST, ERROR, "Failed to open file " << finalFilePath << " for writing: " << ec.message());
                processNext();
                return;
            }

            file.write(body.data(), body.size());
            if (!file)
            {
                std::error_code ec{ errno, std::generic_category() };
                LMS_LOG(PODCAST, ERROR, "Failed to write to file " << finalFilePath << ": " << ec.message());
                processNext();
                return;
            }

            const std::string* contentType{ msg.getHeader("Content-Type") };
            LMS_LOG(PODCAST, INFO, "Downloaded episode artwork for episode '" << episode->getTitle() << "' to " << finalFilePath << " with content type '" << (contentType ? *contentType : "unknown") << "', size = " << body.size() << " bytes");
            createEpisodeArtwork(getDb().getTLSSession(), episodeId, finalFilePath, contentType ? *contentType : "application/octet-stream");

            processNext();
        };
        params.onAbortFunc = [this] {
            onAbort();
        };

        getClient().sendGETRequest(std::move(params));
    }
} // namespace lms::podcast