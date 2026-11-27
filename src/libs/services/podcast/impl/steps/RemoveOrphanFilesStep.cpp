/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "RemoveOrphanFilesStep.hpp"

#include <filesystem>
#include <set>
#include <system_error>
#include <vector>

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"

#include "Utils.hpp"

namespace lms::podcast
{
    namespace
    {
        // Episodes and artworks share the cache directory, under random file names
        std::set<std::filesystem::path> getReferencedFileNames(db::ISession& session)
        {
            std::set<std::filesystem::path> res;

            auto transaction{ session.createReadTransaction() };

            auto addArtwork{ [&](db::ArtworkId artworkId) {
                if (artworkId.isValid())
                    res.insert(db::Artwork::getAbsoluteFilePath(session, artworkId).filename());
            } };

            db::Podcast::find(session, [&](const db::Podcast& podcast) {
                addArtwork(podcast.getArtworkId());
            });

            db::PodcastEpisode::find(session, db::PodcastEpisode::FindParameters{}, [&](const db::PodcastEpisode& episode) {
                addArtwork(episode.getArtworkId());
                if (!episode.getAudioRelativeFilePath().empty())
                    res.insert(episode.getAudioRelativeFilePath());
            });

            return res;
        }
    } // namespace

    core::LiteralString RemoveOrphanFilesStep::getName() const
    {
        return "Remove orphan files";
    }

    void RemoveOrphanFilesStep::run()
    {
        const std::set<std::filesystem::path> referencedFileNames{ getReferencedFileNames(getDb().getTLSSession()) };

        std::vector<std::filesystem::path> orphanFiles;
        std::error_code ec;
        for (std::filesystem::directory_iterator it{ getCachePath(), ec }; !ec && it != std::filesystem::directory_iterator{}; it.increment(ec))
        {
            // also skips the tmp directory
            std::error_code fileEc;
            if (it->is_regular_file(fileEc) && !referencedFileNames.contains(it->path().filename()))
                orphanFiles.push_back(it->path());
        }

        if (ec)
        {
            LMS_LOG(PODCAST, ERROR, "Failed to list cache directory " << getCachePath() << ": " << ec.message());
        }
        else
        {
            for (const std::filesystem::path& orphanFile : orphanFiles)
            {
                LMS_LOG(PODCAST, INFO, "Removing orphan file " << orphanFile);
                utils::removeFile(orphanFile);
            }
        }

        onDone();
    }
} // namespace lms::podcast
