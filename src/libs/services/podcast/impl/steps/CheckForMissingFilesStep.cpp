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
    } // namespace

    core::LiteralString CheckForMissingFilesStep::getName() const
    {
        return "Check for missing files";
    }

    void CheckForMissingFilesStep::run()
    {
        collectMissingImages();
        deleteMissingImages();
        // checkMissingEpisodes();
        onDone();
    }

    void CheckForMissingFilesStep::collectMissingImages()
    {
        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        auto checkArtwork{ [&](const db::Artwork::pointer& artwork) {
            if (!artwork)
                return;

            const auto underlyingImageId{ artwork->getUnderlyingId() };
            const db::ImageId* imageId{ std::get_if<db::ImageId>(&underlyingImageId) };
            if (!imageId)
                return; // these artworks can only be an image

            const std::filesystem::path filePath{ artwork->getAbsoluteFilePath() };
            if (!fileExists(filePath.string()))
            {
                LMS_LOG(PODCAST, DEBUG, "Artwork file is missing: " << filePath);
                _missingImages.push_back(*imageId);
            }
        } };

        db::Podcast::find(session, [&](const db::Podcast::pointer& podcast) {
            checkArtwork(podcast->getArtwork());
        });

        db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode::pointer& episode) {
            checkArtwork(episode->getArtwork());
        });
    }

    void CheckForMissingFilesStep::deleteMissingImages()
    {
        if (_missingImages.empty())
            return;

        auto& session{ getDb().getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        session.destroy<db::Image>(_missingImages); // will propagate to artworks and podcasts/episodes
    }
} // namespace lms::podcast