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

#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"

#include "LmsApplication.hpp"
#include "PlayQueue.hpp"
#include "explore/Filters.hpp"

namespace lms::ui
{
    namespace
    {
        std::vector<db::TrackId> getArtistsTracks(db::Session& session, const std::vector<db::ArtistId>& artistsId, const Filters& filters, std::size_t maxTrackCount)
        {
            assert(maxTrackCount);

            std::vector<db::TrackId> res;

            auto transaction{ session.createReadTransaction() };

            for (const db::ArtistId artistId : artistsId)
            {
                db::Track::FindParameters params;
                params.setArtist(artistId);
                params.setSortMethod(db::TrackSortMethod::DateDescAndRelease);
                params.setFilters(filters.getDbFilters());
                params.setRange(db::Range{ 0, maxTrackCount - res.size() });

                const auto tracks{ db::Track::findIds(session, params) };

                res.reserve(res.size() + tracks.results.size());
                res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

                if (res.size() == maxTrackCount)
                    break;
            }

            return res;
        }

        std::vector<db::TrackId> getReleasesTracks(db::Session& session, const std::vector<db::ReleaseId>& releasesId, const Filters& filters, std::size_t maxTrackCount)
        {
            using namespace db;
            assert(maxTrackCount);

            std::vector<TrackId> res;

            auto transaction{ session.createReadTransaction() };

            for (const ReleaseId releaseId : releasesId)
            {
                db::Track::FindParameters params;
                params.setRelease(releaseId);
                params.setSortMethod(db::TrackSortMethod::Release);
                params.setFilters(filters.getDbFilters());
                params.setRange(db::Range{ 0, maxTrackCount - res.size() });

                const auto tracks{ db::Track::findIds(session, params) };

                res.reserve(res.size() + tracks.results.size());
                res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

                if (res.size() == maxTrackCount)
                    break;
            }

            return res;
        }

        std::vector<db::TrackId> getDiscTracks(db::Session& session, const std::vector<PlayQueueController::Disc>& discs, const Filters& filters, std::size_t maxTrackCount)
        {
            using namespace db;
            assert(maxTrackCount);

            std::vector<TrackId> res;

            auto transaction{ session.createReadTransaction() };

            for (const PlayQueueController::Disc& disc : discs)
            {
                db::Track::FindParameters params;
                params.setRelease(disc.releaseId);
                params.setSortMethod(db::TrackSortMethod::Release);
                params.setDiscNumber(disc.discNumber);
                params.setFilters(filters.getDbFilters());
                params.setRange(db::Range{ 0, maxTrackCount - res.size() });

                const auto tracks{ db::Track::findIds(session, params) };

                res.reserve(res.size() + tracks.results.size());
                res.insert(std::end(res), std::cbegin(tracks.results), std::cend(tracks.results));

                if (res.size() == maxTrackCount)
                    break;
            }

            return res;
        }

        std::vector<db::TrackId> getTrackListTracks(db::Session& session, db::TrackListId trackListId, const Filters& filters, std::size_t maxTrackCount)
        {
            using namespace db;
            assert(maxTrackCount);

            auto transaction{ session.createReadTransaction() };

            db::Track::FindParameters params;
            params.setTrackList(trackListId);
            params.setFilters(filters.getDbFilters());
            params.setRange(db::Range{ 0, maxTrackCount });
            params.setSortMethod(TrackSortMethod::TrackList);

            return db::Track::findIds(session, params).results;
        }
    } // namespace

    PlayQueueController::PlayQueueController(Filters& filters, PlayQueue& playQueue)
        : _filters{ filters }
        , _playQueue{ playQueue }
    {
    }

    void PlayQueueController::processCommand(Command command, const std::vector<db::ArtistId>& artistIds)
    {
        const std::vector<db::TrackId> tracks{ getArtistsTracks(LmsApp->getDbSession(), artistIds, _filters, _maxTrackCountToEnqueue) };
        processCommand(command, tracks);
    }

    void PlayQueueController::processCommand(Command command, const std::vector<db::ReleaseId>& releaseIds)
    {
        const std::vector<db::TrackId> tracks{ getReleasesTracks(LmsApp->getDbSession(), releaseIds, _filters, _maxTrackCountToEnqueue) };
        processCommand(command, tracks);
    }

    void PlayQueueController::processCommand(Command command, const std::vector<db::TrackId>& trackIds)
    {
        // consider things are already filtered here, and _maxTrackCount honored playqueue side...
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

    void PlayQueueController::processCommand(Command command, db::TrackListId trackListId)
    {
        const std::vector<db::TrackId> tracks{ getTrackListTracks(LmsApp->getDbSession(), trackListId, _filters, _maxTrackCountToEnqueue) };
        processCommand(command, tracks);
    }

    void PlayQueueController::processCommand(Command command, const std::vector<Disc>& discs)
    {
        const std::vector<db::TrackId> tracks{ getDiscTracks(LmsApp->getDbSession(), discs, _filters, _maxTrackCountToEnqueue) };
        processCommand(command, tracks);
    }

    void PlayQueueController::playTrackInRelease(db::TrackId trackId)
    {
        db::ReleaseId releaseId;
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };
            const db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
            if (!track || !track->getRelease())
                return;

            releaseId = track->getRelease()->getId();
        }

        const std::vector<db::TrackId> tracks{ getReleasesTracks(LmsApp->getDbSession(), { releaseId }, _filters, _maxTrackCountToEnqueue) };
        auto itTrack{ std::find(std::cbegin(tracks), std::cend(tracks), trackId) };
        if (itTrack == std::cend(tracks))
            return;

        const std::size_t index{ static_cast<std::size_t>(std::distance(std::cbegin(tracks), itTrack)) };
        _playQueue.playAtIndex(tracks, index);
    }
} // namespace lms::ui
