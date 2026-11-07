/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "explore/PlayQueueController.hpp"

#include "database/ISession.hpp"
#include "database/objects/Track.hpp"

#include "LmsApplication.hpp"
#include "PlayQueue.hpp"

namespace lms::ui
{
    namespace
    {
        std::vector<db::TrackId> getReleasesTracks(db::ISession& session, std::span<const db::ReleaseId> releasesId)
        {
            std::vector<db::TrackId> res;

            auto transaction{ session.createReadTransaction() };

            for (const db::ReleaseId releaseId : releasesId)
            {
                db::Track::FindParameters params;
                params.setRelease(releaseId);
                params.setSortMethod(db::TrackSortMethod::Release);

                const auto tracks{ db::Track::findIds(session, params) };

                res.reserve(res.size() + tracks.size());
                res.insert(std::end(res), std::cbegin(tracks), std::cend(tracks));
            }

            return res;
        }

    } // namespace

    PlayQueueController::PlayQueueController(PlayQueue& playQueue)
        : _playQueue{ playQueue }
    {
    }

    void PlayQueueController::processCommand(Command command, std::span<const db::ReleaseId> releaseIds)
    {
        const std::vector<db::TrackId> tracks{ getReleasesTracks(LmsApp->getDbSession(), releaseIds) };
        processCommand(command, tracks);
    }

    void PlayQueueController::processCommand(Command command, std::span<const db::TrackId> trackIds)
    {
        switch (command)
        {
        case Command::Play:
            _playQueue.play(trackIds);
            break;
        case Command::PlayNext:
            _playQueue.playNext(trackIds);
            break;
        case Command::PlayShuffled:
            _playQueue.playShuffled(trackIds);
            break;
        case Command::PlayOrAddLast:
            _playQueue.playOrAddLast(trackIds);
            break;
        }
    }

    void PlayQueueController::playAtIndex(std::span<const db::TrackId> trackIds, std::size_t index)
    {
        _playQueue.playAtIndex(trackIds, index);
    }
} // namespace lms::ui
