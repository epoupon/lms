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

#include <array>

#include "core/ILogger.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "Executor.hpp"
#include "PodcastParsing.hpp"
#include "PodcastTypes.hpp"
#include "UrlValidation.hpp"

namespace lms::podcast
{
    namespace
    {
        void removeArtwork(db::ISession& session, const db::Artwork& artwork)
        {
            assert(artwork.getImageId().isValid()); // these artworks can only be an image

            const std::filesystem::path filePath{ db::Artwork::getAbsoluteFilePath(session, artwork.getId()) };
            std::error_code ec;
            std::filesystem::remove(filePath, ec);
            if (ec)
                LMS_LOG(PODCAST, WARNING, "Failed to remove old podcast artwork file '" << filePath << "': " << ec.message());

            db::Image::destroy(session, std::array{ artwork.getImageId() });
        }

        void addEpisode(db::ISession& session, const db::Podcast& dbPodcast, const PodcastEpisode& episode)
        {
            LMS_LOG(PODCAST, DEBUG, "Adding episode '" << episode.title << "' to podcast '" << dbPodcast.getTitle() << "'");

            db::PodcastEpisode dbEpisode{ db::PodcastEpisode::create(session, dbPodcast.getId()) };

            dbEpisode.setAuthor(episode.author);
            dbEpisode.setCategory(episode.category);
            dbEpisode.setDescription(episode.description);

            if (isAllowedPodcastUrl(episode.enclosureUrl.url))
            {
                dbEpisode.setEnclosureUrl(episode.enclosureUrl.url);
                dbEpisode.setEnclosureContentType(episode.enclosureUrl.type);
                dbEpisode.setEnclosureLength(episode.enclosureUrl.length);
            }
            else
            {
                LMS_LOG(PODCAST, WARNING, "Episode '" << episode.title << "' : ignoring enclosure URL '" << episode.enclosureUrl.url << "' (bad URL)");
            }

            dbEpisode.setExplicit(episode.explicitContent ? *episode.explicitContent : false);
            dbEpisode.setLink(episode.link);
            dbEpisode.setPubDate(episode.pubDate);
            dbEpisode.setTitle(episode.title);

            if (isAllowedPodcastUrl(episode.imageUrl))
                dbEpisode.setImageUrl(episode.imageUrl);
            else if (!episode.imageUrl.empty())
                LMS_LOG(PODCAST, WARNING, "Episode '" << episode.title << "' : ignoring image URL '" << episode.imageUrl << "' (bad URL)");

            dbEpisode.setDuration(episode.duration);
            db::PodcastEpisode::update(session, dbEpisode);
        }

        void updatePodcast(db::ISession& session, db::PodcastId podcastId, const Podcast& podcast)
        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<db::Podcast> dbPodcast{ db::Podcast::find(session, podcastId) };
            if (!dbPodcast)
                return; // may have been deleted by admin

            LMS_LOG(PODCAST, DEBUG, "Refreshing podcast '" << podcast.title << "' received from '" << dbPodcast->getUrl() << "'");

            // force update the podcast data
            if (!podcast.newUrl.empty() && podcast.newUrl != dbPodcast->getUrl())
            {
                if (isAllowedPodcastUrl(podcast.newUrl))
                {
                    LMS_LOG(PODCAST, INFO, "Podcast '" << podcast.title << "' : URL changed from '" << dbPodcast->getUrl() << "' to '" << podcast.newUrl << "'");
                    dbPodcast->setUrl(podcast.newUrl);
                }
                else
                {
                    LMS_LOG(PODCAST, WARNING, "Podcast '" << podcast.title << "' : ignoring new podcast URL '" << podcast.newUrl << "' (bad URL)");
                }
            }
            dbPodcast->setAuthor(podcast.author);
            dbPodcast->setCategory(podcast.category);
            dbPodcast->setCopyright(podcast.copyright);
            dbPodcast->setDescription(podcast.description);
            dbPodcast->setExplicit(podcast.explicitContent ? *podcast.explicitContent : false);
            dbPodcast->setLanguage(podcast.language);
            dbPodcast->setLastBuildDate(podcast.lastBuildDate);
            dbPodcast->setLink(podcast.link);
            dbPodcast->setOwnerEmail(podcast.ownerEmail);
            dbPodcast->setOwnerName(podcast.ownerName);
            dbPodcast->setSubtitle(podcast.subtitle);
            dbPodcast->setSummary(podcast.summary);
            dbPodcast->setTitle(podcast.title);
            if (std::string previousUrl{ dbPodcast->getImageUrl() }; !previousUrl.empty() && previousUrl != podcast.imageUrl)
            {
                if (isAllowedPodcastUrl(podcast.imageUrl))
                {
                    LMS_LOG(PODCAST, INFO, "Podcast '" << podcast.title << "' : image url changed from '" << previousUrl << "' to '" << podcast.imageUrl << "'");
                    if (const std::optional<db::Artwork> currentArtwork{ db::Artwork::find(session, dbPodcast->getArtworkId()) })
                    {
                        removeArtwork(session, *currentArtwork);
                        dbPodcast->setArtwork(db::ArtworkId{});
                    }

                    dbPodcast->setImageUrl(podcast.imageUrl);
                }
                else
                {
                    LMS_LOG(PODCAST, WARNING, "Podcast '" << podcast.title << "' : ignoring image URL '" << podcast.imageUrl << "' (disallowed scheme)");
                }
            }

            db::Podcast::update(session, *dbPodcast);

            // Only create episodes if they are new, do not modify/update existing entries for now
            // TODO: update existing episodes, remove artwork if url changed
            Wt::WDateTime previousNewestEpisodeDateTime{};
            if (std::optional<db::PodcastEpisode> dbEpisode{ db::PodcastEpisode::findNewtestEpisode(session, podcastId) })
                previousNewestEpisodeDateTime = dbEpisode->getPubDate();

            // TODO: mark for deletion old episodes that are no longer referenced!!
            for (const auto& episode : podcast.episodes)
            {
                if (previousNewestEpisodeDateTime.isValid() && episode.pubDate <= previousNewestEpisodeDateTime)
                    continue; // consider already in db

                addEpisode(session, *dbPodcast, episode);
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

        db::Podcast::find(session, [this](const db::Podcast& podcast) {
            LMS_LOG(PODCAST, DEBUG, "Found podcast to refresh at '" << podcast.getUrl() << "'");
            podcastsToRefresh.push(podcast.getId());
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

        const std::optional<db::Podcast> podcast{ db::Podcast::find(session, podcastId) };
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