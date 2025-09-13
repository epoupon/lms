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

#include "CheckForMissingFilesStep.hpp"

#include <vector>

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

namespace lms::podcast
{
    namespace
    {
        bool fileExists(const std::string& path)
        {
            std::error_code ec;
            bool res{ std::filesystem::exists(path, ec) };
            if (ec)
            {
                LMS_LOG(PODCAST, ERROR, "Error checking file existence for path " << path << ": " << ec.message());
                return false;
            }

            return res;
        }

        bool checkArtworkFile(const db::Artwork::pointer& artwork)
        {
            const auto underlyingImageId{ artwork->getUnderlyingId() };
            const db::ImageId* imageId{ std::get_if<db::ImageId>(&underlyingImageId) };
            assert(imageId); // these artworks can only be an image

            const std::filesystem::path filePath{ artwork->getAbsoluteFilePath() };
            if (!fileExists(filePath.string()))
            {
                LMS_LOG(PODCAST, DEBUG, "Artwork file is missing: " << filePath);
                return false;
            }

            return true;
        }
    } // namespace

    core::LiteralString CheckForMissingFilesStep::getName() const
    {
        return "Check for missing files";
    }

    void CheckForMissingFilesStep::run()
    {
        checkMissingImages();
        checkMissingEpisodes();

        onDone();
    }

    void CheckForMissingFilesStep::checkMissingImages()
    {
        std::vector<db::ImageId> missingImages;

        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::Podcast::find(session, [&](const db::Podcast::pointer& podcast) {
                if (const db::Artwork::pointer artwork{ podcast->getArtwork() })
                {
                    if (!checkArtworkFile(artwork))
                        missingImages.push_back(artwork->getImageId());
                }
            });

            db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode::pointer& episode) {
                if (const db::Artwork::pointer artwork{ episode->getArtwork() })
                {
                    if (!checkArtworkFile(artwork))
                        missingImages.push_back(artwork->getImageId());
                }
            });
        }

        if (!missingImages.empty())
        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            session.destroy<db::Image>(missingImages); // will propagate to artworks and podcasts/episodes
        }
    }

    void CheckForMissingFilesStep::checkMissingEpisodes()
    {
        std::vector<db::PodcastEpisodeId> missingEpisodes;

        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode::pointer& episode) {
                if (episode->getAudioRelativeFilePath().empty())
                    return;

                const std::filesystem::path filePath{ getCachePath() / episode->getAudioRelativeFilePath() };
                if (!fileExists(filePath))
                {
                    LMS_LOG(PODCAST, INFO, "Episode file " << filePath << " is missing for episode '" << episode->getTitle() << "'");
                    missingEpisodes.push_back(episode->getId());
                }
            });
        }

        if (!missingEpisodes.empty())
        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            for (const auto& episodeId : missingEpisodes)
            {
                db::PodcastEpisode::pointer episode{ db::PodcastEpisode::find(session, episodeId) };
                episode.modify()->setAudioRelativeFilePath({});
            }
        }
    }

} // namespace lms::podcast