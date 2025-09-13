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

#include "RemovePodcastsStep.hpp"

#include <filesystem>
#include <vector>

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "Utils.hpp"

namespace lms::podcast
{

    core::LiteralString RemovePodcastsStep::getName() const
    {
        return "Remove podcasts";
    }

    void RemovePodcastsStep::run()
    {
        std::vector<db::PodcastId> podcastsToRemove;
        std::vector<db::ImageId> imagesToRemove;

        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::Podcast::find(session, [&](const db::Podcast::pointer& podcast) {
                if (!podcast->isDeleteRequested())
                    return;

                LMS_LOG(PODCAST, DEBUG, "Removing podcast '" << podcast->getUrl() << "'. Title: '" << podcast->getTitle() << "'");

                // remove podcast artwork
                if (const db::Artwork::pointer artwork{ podcast->getArtwork() })
                {
                    utils::removeFile(artwork->getAbsoluteFilePath());
                    imagesToRemove.emplace_back(std::get<db::ImageId>(artwork->getUnderlyingId()));
                }

                db::PodcastEpisode::FindParameters params;
                params.setPodcast(podcast->getId());

                db::PodcastEpisode::find(session, params, [&](const db::PodcastEpisode::pointer& episode) {
                    if (const db::Artwork::pointer artwork{ episode->getArtwork() })
                    {
                        utils::removeFile(artwork->getAbsoluteFilePath());
                        imagesToRemove.emplace_back(std::get<db::ImageId>(artwork->getUnderlyingId()));
                    }

                    if (!episode->getAudioRelativeFilePath().empty())
                        utils::removeFile(getCachePath() / episode->getAudioRelativeFilePath());
                });

                podcastsToRemove.emplace_back(podcast->getId());
            });
        }

        // second step, remove the database entries (must be consistent with first step!)
        if (!podcastsToRemove.empty() || !imagesToRemove.empty())
        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            session.destroy<db::Podcast>(podcastsToRemove); // will propagate to episodes
            session.destroy<db::Image>(imagesToRemove);     // will propagate to artworks
        }

        onDone();
    }
} // namespace lms::podcast