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

#include "RefreshPodcastsStep.hpp"

#include "core/ILogger.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "Executor.hpp"
#include "PodcastParsing.hpp"
#include "PodcastTypes.hpp"

namespace lms::podcast
{
    namespace
    {
        void removeArtwork(db::Session& session, const db::Artwork::pointer& artwork)
        {
            const auto underlyingImageId{ artwork->getUnderlyingId() };
            const auto* imageId{ std::get_if<db::ImageId>(&underlyingImageId) };
            assert(imageId); // these artworks can only be an image

            std::error_code ec;
            std::filesystem::remove(artwork->getAbsoluteFilePath(), ec);
            if (ec)
                LMS_LOG(PODCAST, WARNING, "Failed to remove old podcast artwork file '" << artwork->getAbsoluteFilePath() << "': " << ec.message());

            session.destroy<db::Image>(*imageId);
        }

        void updatePodcast(db::Session& session, db::PodcastId podcastId, const Podcast& podcast)
        {
            auto transaction{ session.createWriteTransaction() };

            db::Podcast::pointer dbPodcast{ db::Podcast::find(session, podcastId) };
            if (!dbPodcast)
                return; // may have been deleted by admin

            LMS_LOG(PODCAST, DEBUG, "Refreshing podcast '" << podcast.title << "' received from '" << dbPodcast->getUrl() << "'");

            // force update the podcast data
            if (!podcast.newUrl.empty() && podcast.newUrl != dbPodcast->getUrl())
            {
                LMS_LOG(PODCAST, INFO, "Podcast '" << podcast.title << "' : URL changed from '" << dbPodcast->getUrl() << "' to '" << podcast.newUrl << "'");
                dbPodcast.modify()->setUrl(podcast.newUrl);
            }
            dbPodcast.modify()->setAuthor(podcast.author);
            dbPodcast.modify()->setCategory(podcast.category);
            dbPodcast.modify()->setCopyright(podcast.copyright);
            dbPodcast.modify()->setDescription(podcast.description);
            dbPodcast.modify()->setExplicit(podcast.explicitContent ? *podcast.explicitContent : false);
            dbPodcast.modify()->setLanguage(podcast.language);
            dbPodcast.modify()->setLastBuildDate(podcast.lastBuildDate);
            dbPodcast.modify()->setLink(podcast.link);
            dbPodcast.modify()->setOwnerEmail(podcast.ownerEmail);
            dbPodcast.modify()->setOwnerName(podcast.ownerName);
            dbPodcast.modify()->setSubtitle(podcast.subtitle);
            dbPodcast.modify()->setSummary(podcast.summary);
            dbPodcast.modify()->setTitle(podcast.title);
            if (std::string previousUrl{ dbPodcast->getImageUrl() }; !previousUrl.empty() && previousUrl != podcast.imageUrl)
            {
                LMS_LOG(PODCAST, INFO, "Podcast '" << podcast.title << "' : image url changed from '" << previousUrl << "' to '" << podcast.imageUrl << "'");
                if (db::Artwork::pointer currentArtwork{ dbPodcast->getArtwork() })
                    removeArtwork(session, currentArtwork);

                dbPodcast.modify()->setImageUrl(podcast.imageUrl);
            }

            // Only create episodes if they are new, do not modify/update existing entries for now
            // TODO: update existing episodes, remove artwork if url changed
            Wt::WDateTime previousNewestEpisodeDateTime{};
            if (db::PodcastEpisode::pointer dbEpisode{ db::PodcastEpisode::findNewtestEpisode(session, podcastId) })
                previousNewestEpisodeDateTime = dbEpisode->getPubDate();

            // TODO: mark for deletion old episodes that are no longer referenced!!
            for (const auto& episode : podcast.episodes)
            {
                if (previousNewestEpisodeDateTime.isValid() && episode.pubDate <= previousNewestEpisodeDateTime)
                    continue; // consider already in db

                LMS_LOG(PODCAST, DEBUG, "Adding episode '" << episode.title << "' to podcast '" << podcast.title << "'");

                auto dbEpisode{ session.create<db::PodcastEpisode>(dbPodcast) };

                dbEpisode.modify()->setAuthor(episode.author);
                dbEpisode.modify()->setCategory(episode.category);
                dbEpisode.modify()->setDescription(episode.description);
                dbEpisode.modify()->setEnclosureUrl(episode.enclosureUrl.url);
                dbEpisode.modify()->setEnclosureContentType(episode.enclosureUrl.type);
                dbEpisode.modify()->setEnclosureLength(episode.enclosureUrl.length);
                dbEpisode.modify()->setExplicit(episode.explicitContent ? *episode.explicitContent : false);
                dbEpisode.modify()->setLink(episode.link);
                dbEpisode.modify()->setPubDate(episode.pubDate);
                dbEpisode.modify()->setTitle(episode.title);
                dbEpisode.modify()->setImageUrl(episode.imageUrl);
                dbEpisode.modify()->setDuration(episode.duration);
            }
        }
    } // namespace

    core::LiteralString RefreshPodcastsStep::getName() const
    {
        return "Refresh podcasts";
    }

    void RefreshPodcastsStep::run()
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        db::Podcast::find(session, [this](const db::Podcast::pointer& podcast) {
            LMS_LOG(PODCAST, DEBUG, "Found podcast to refresh at '" << podcast->getUrl() << "'");
            podcastsToRefresh.push(podcast->getId());
        });

        refreshNextPodcast();
    }

    void RefreshPodcastsStep::refreshNextPodcast()
    {
        if (abortRequested())
        {
            onAbort();
            return;
        }

        getExecutor().post([this] {
            if (podcastsToRefresh.empty())
            {
                LMS_LOG(PODCAST, DEBUG, "All podcasts refreshed");
                onDone();
                return;
            }

            const db::PodcastId podcastId{ podcastsToRefresh.front() };
            podcastsToRefresh.pop();
            refreshPodcast(podcastId);
        });
    }

    void RefreshPodcastsStep::refreshPodcast(db::PodcastId podcastId)
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const db::Podcast::pointer podcast{ db::Podcast::find(session, podcastId) };
        if (!podcast)
        {
            refreshNextPodcast(); // maybe removed in the meantime by admin
            return;
        }

        LMS_LOG(PODCAST, DEBUG, "Syncing podcast from '" << podcast->getUrl() << "'");

        const std::string url{ podcast->getUrl() };
        core::http::ClientGETRequestParameters params;
        params.relativeUrl = podcast->getUrl();
        params.onFailureFunc = [this, podcast] {
            LMS_LOG(PODCAST, ERROR, "Failed to sync podcast from '" << podcast->getUrl() << "'");
            refreshNextPodcast();
        };
        params.onSuccessFunc = [this, podcast, podcastId](const Wt::Http::Message& msg) {
            getExecutor().post([this, podcast, podcastId, msgBody = msg.body()] {
                try
                {
                    const auto podcast{ parsePodcastRssFeed(msgBody) };
                    updatePodcast(getDb().getTLSSession(), podcastId, podcast);
                }
                catch (const ParseException& e)
                {
                    LMS_LOG(PODCAST, ERROR, "Failed to parse rss feed from '" << podcast->getUrl() << "': " << e.what());
                }
                refreshNextPodcast();
            });
        };
        params.onAbortFunc = [this] {
            onAbort();
        };

        getClient().sendGETRequest(std::move(params));
    }
} // namespace lms::podcast