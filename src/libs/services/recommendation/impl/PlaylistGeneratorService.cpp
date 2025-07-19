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

#include "PlaylistGeneratorService.hpp"

#include <algorithm>

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "services/recommendation/IRecommendationService.hpp"

#include "playlist-constraints/ConsecutiveArtists.hpp"
#include "playlist-constraints/ConsecutiveReleases.hpp"
#include "playlist-constraints/DuplicateTracks.hpp"

namespace lms::recommendation
{
    using namespace db;

    std::unique_ptr<IPlaylistGeneratorService> createPlaylistGeneratorService(db::IDb& db, IRecommendationService& recommendationService)
    {
        return std::make_unique<PlaylistGeneratorService>(db, recommendationService);
    }

    PlaylistGeneratorService::PlaylistGeneratorService(db::IDb& db, IRecommendationService& recommendationService)
        : _db{ db }
        , _recommendationService{ recommendationService }
    {
        _constraints.push_back(std::make_unique<PlaylistGeneratorConstraint::ConsecutiveArtists>(_db));
        _constraints.push_back(std::make_unique<PlaylistGeneratorConstraint::ConsecutiveReleases>(_db));
        _constraints.push_back(std::make_unique<PlaylistGeneratorConstraint::DuplicateTracks>());
    }

    std::vector<TrackId> PlaylistGeneratorService::extendPlaylist(TrackListId tracklistId, std::size_t maxCount) const
    {
        LMS_LOG(RECOMMENDATION, DEBUG, "Requested to extend playlist by " << maxCount << " similar tracks");

        // supposed to be ordered from most similar to least similar
        std::vector<TrackId> similarTracks{ _recommendationService.findSimilarTracks(tracklistId, maxCount * 2) }; // ask for more tracks than we need as it will be easier to respect constraints

        const std::vector<TrackId> startingTracks{ getTracksFromTrackList(tracklistId) };

        std::vector<TrackId> finalResult = startingTracks;
        finalResult.reserve(startingTracks.size() + maxCount);

        std::vector<float> scores;
        for (std::size_t i{}; i < maxCount; ++i)
        {
            if (similarTracks.empty())
                break;

            scores.resize(similarTracks.size(), {});

            // select the similar track that has the best score
            for (std::size_t trackIndex{}; trackIndex < similarTracks.size(); ++trackIndex)
            {
                using namespace db::Debug;

                finalResult.push_back(similarTracks[trackIndex]);

                scores[trackIndex] = 0;
                for (const auto& constraint : _constraints)
                    scores[trackIndex] += constraint->computeScore(finalResult, finalResult.size() - 1);

                finalResult.pop_back();

                // early exit if we consider we found a track with no constraint violation (since similarTracks sorted from most to least similar)
                if (scores[trackIndex] < 0.01)
                    break;
            }

            // get the best score
            const std::size_t bestScoreIndex{ static_cast<std::size_t>(std::distance(std::cbegin(scores), std::min_element(std::cbegin(scores), std::cend(scores)))) };

            finalResult.push_back(similarTracks[bestScoreIndex]);
            similarTracks.erase(std::begin(similarTracks) + bestScoreIndex);
        }

        // for now, just get some more similar tracks
        return std::vector(std::cbegin(finalResult) + startingTracks.size(), std::cend(finalResult));
    }

    TrackContainer PlaylistGeneratorService::getTracksFromTrackList(db::TrackListId tracklistId) const
    {
        TrackContainer tracks;

        Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        Track::FindParameters params;
        params.setTrackList(tracklistId);
        params.setSortMethod(TrackSortMethod::TrackList);

        for (const TrackId trackId : Track::findIds(dbSession, params).results)
            tracks.push_back(trackId);

        return tracks;
    }
} // namespace lms::recommendation
