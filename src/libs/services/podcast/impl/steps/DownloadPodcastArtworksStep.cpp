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

#include "DownloadPodcastArtworksStep.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "core/ILogger.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Podcast.hpp"

#include "Executor.hpp"
#include "Utils.hpp"

namespace lms::podcast
{
    namespace
    {
        void createPodcastArtwork(db::Session& session, db::PodcastId podcastId, const std::filesystem::path& filePath, std::string_view contentType)
        {
            auto transaction{ session.createWriteTransaction() };

            db::Podcast::pointer dbPodcast{ db::Podcast::find(session, podcastId) };
            if (!dbPodcast)
                return; // may have been deleted by admin

            if (db::Artwork::pointer artwork{ utils::createArtworkFromImage(session, filePath, contentType) })
                dbPodcast.modify()->setArtwork(artwork);
        }
    } // namespace

    core::LiteralString DownloadPodcastArtworksStep::getName() const
    {
        return "Download podcast artworks";
    }

    void DownloadPodcastArtworksStep::run()
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        _podcastArtworksToDownload.clear();
        db::Podcast::find(session, [&](const db::Podcast::pointer& podcast) {
            if (podcast->getImageUrl().empty() || podcast->getTitle().empty())
                return;

            if (podcast->getArtworkId().isValid())
                return;

            _podcastArtworksToDownload.push_back(podcast->getId());
        });

        processNext();
    }

    void DownloadPodcastArtworksStep::processNext()
    {
        if (abortRequested())
        {
            onAbort();
            return;
        }

        getExecutor().post([this] {
            if (_podcastArtworksToDownload.empty())
            {
                onDone();
                return;
            }

            const db::PodcastId podcastId{ _podcastArtworksToDownload.front() };
            _podcastArtworksToDownload.pop_front();
            process(podcastId);
        });
    }

    void DownloadPodcastArtworksStep::process(db::PodcastId podcastId)
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const auto podcast{ db::Podcast::find(getDb().getTLSSession(), podcastId) };
        if (!podcast)
        {
            LMS_LOG(PODCAST, DEBUG, "Cannot find podcast: removed?");
            processNext();
            return;
        }

        const std::string url{ podcast->getImageUrl() };
        const std::string randomName{ utils::generateRandomFileName() };
        const std::filesystem::path tmpFilePath{ getTmpCachePath() / randomName };
        const std::filesystem::path finalFilePath{ getCachePath() / randomName };

        core::http::ClientGETRequestParameters params;
        params.relativeUrl = podcast->getImageUrl();
        params.onChunkReceived = [url, tmpFilePath, this](std::span<const std::byte> chunk) {
            if (abortRequested())
                return core::http::ClientGETRequestParameters::ChunckReceivedResult::Abort;

            std::ofstream file{ tmpFilePath, std::ios::binary | std::ios::app };
            if (!file)
            {
                std::error_code ec{ errno, std::generic_category() };
                LMS_LOG(PODCAST, ERROR, "Failed to open file " << tmpFilePath << " for writing: " << ec.message());
                return core::http::ClientGETRequestParameters::ChunckReceivedResult::Abort;
            }

            file.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            if (!file)
            {
                std::error_code ec{ errno, std::generic_category() };
                LMS_LOG(PODCAST, ERROR, "Failed to write to file " << tmpFilePath << ": " << ec.message());
                return core::http::ClientGETRequestParameters::ChunckReceivedResult::Abort;
            }

            return core::http::ClientGETRequestParameters::ChunckReceivedResult::Continue;
        };
        params.onFailureFunc = [this, podcast] {
            LMS_LOG(PODCAST, ERROR, "Failed to download podcast image from '" << podcast->getImageUrl() << "'");
            processNext();
        };
        params.onSuccessFunc = [=, this](const Wt::Http::Message& msg) {
            std::error_code ec;
            std::filesystem::rename(tmpFilePath, finalFilePath, ec);
            if (ec)
            {
                LMS_LOG(PODCAST, ERROR, "Failed to rename tmp podcast artwork file " << tmpFilePath << " to final location " << finalFilePath << ": " << ec.message());
                processNext();
                return;
            }

            const std::string* contentType{ msg.getHeader("Content-Type") };

            LMS_LOG(PODCAST, INFO, "Downloaded podcast artwork for podcast '" << podcast->getTitle() << "' to " << finalFilePath << " with content type '" << (contentType ? *contentType : "unknown") << "'");
            createPodcastArtwork(getDb().getTLSSession(), podcastId, finalFilePath, contentType ? *contentType : "application/octet-stream");

            processNext();
        };
        params.onAbortFunc = [this] {
            if (abortRequested())
                onAbort();
            else
                processNext();
        };

        getClient().sendGETRequest(std::move(params));
    }
} // namespace lms::podcast