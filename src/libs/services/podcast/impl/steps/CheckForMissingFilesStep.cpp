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
#include "database/ISession.hpp"
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

        bool checkArtworkFile(db::ISession& session, const db::Artwork& artwork)
        {
            assert(std::holds_alternative<db::ImageId>(artwork.getUnderlyingId())); // these artworks can only be an image

            const std::filesystem::path filePath{ db::Artwork::getAbsoluteFilePath(session, artwork.getId()) };
            return fileExists(filePath.string());
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

            db::Podcast::find(session, [&](const db::Podcast& podcast) {
                if (const std::optional<db::Artwork> artwork{ db::Artwork::find(session, podcast.getArtworkId()) })
                {
                    if (!checkArtworkFile(session, *artwork))
                    {
                        LMS_LOG(PODCAST, DEBUG, "Artwork file " << db::Artwork::getAbsoluteFilePath(session, artwork->getId()) << " is missing for podcast '" << podcast.getTitle() << "'");
                        missingImages.push_back(artwork->getImageId());
                    }
                }
            });

            db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode& episode) {
                if (const std::optional<db::Artwork> artwork{ db::Artwork::find(session, episode.getArtworkId()) })
                {
                    if (!checkArtworkFile(session, *artwork))
                    {
                        LMS_LOG(PODCAST, DEBUG, "Artwork file " << db::Artwork::getAbsoluteFilePath(session, artwork->getId()) << " is missing for podcast episode '" << episode.getTitle() << "'");
                        missingImages.push_back(artwork->getImageId());
                    }
                }
            });
        }

        if (!missingImages.empty())
        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            db::Image::destroy(session, missingImages); // will propagate to artworks and podcasts/episodes
        }
    }

    void CheckForMissingFilesStep::checkMissingEpisodes()
    {
        std::vector<db::PodcastEpisodeId> missingEpisodes;

        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode& episode) {
                if (episode.getAudioRelativeFilePath().empty())
                    return;

                const std::filesystem::path filePath{ getCachePath() / episode.getAudioRelativeFilePath() };
                if (!fileExists(filePath))
                {
                    LMS_LOG(PODCAST, INFO, "Episode file " << filePath << " is missing for episode '" << episode.getTitle() << "'");
                    missingEpisodes.push_back(episode.getId());
                }
            });
        }

        if (!missingEpisodes.empty())
        {
            auto& session{ getDb().getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            for (const auto& episodeId : missingEpisodes)
            {
                std::optional<db::PodcastEpisode> episode{ db::PodcastEpisode::find(session, episodeId) };
                episode->setAudioRelativeFilePath({});
                db::PodcastEpisode::update(session, *episode);
            }
        }
    }
} // namespace lms::podcast