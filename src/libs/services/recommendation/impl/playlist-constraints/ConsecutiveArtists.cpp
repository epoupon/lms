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

#include "ConsecutiveArtists.hpp"

#include <algorithm>

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"

namespace lms::recommendation::PlaylistGeneratorConstraint
{
    namespace
    {
        std::size_t countCommonArtists(const ArtistContainer& artists1, const ArtistContainer& artists2)
        {
            ArtistContainer intersection;

            std::set_intersection(std::cbegin(artists1), std::cend(artists1),
                std::cbegin(artists2), std::cend(artists2),
                std::back_inserter(intersection));

            return intersection.size();
        }
    } // namespace

    ConsecutiveArtists::ConsecutiveArtists(db::IDb& db)
        : _db{ db }
    {
    }

    float ConsecutiveArtists::computeScore(const std::vector<db::TrackId>& trackIds, std::size_t trackIndex)
    {
        assert(!trackIds.empty());
        assert(trackIndex <= trackIds.size() - 1);

        const ArtistContainer artists{ getArtists(trackIds[trackIndex]) };

        constexpr std::size_t rangeSize{ 3 }; // check up to rangeSize tracks before/after the target track
        static_assert(rangeSize > 0);

        float score{};
        for (std::size_t i{ 1 }; i < rangeSize; ++i)
        {
            if (trackIndex >= i)
                score += countCommonArtists(artists, getArtists(trackIds[trackIndex - i])) / static_cast<float>(i);

            if (trackIndex + i < trackIds.size())
                score += countCommonArtists(artists, getArtists(trackIds[trackIndex + i])) / static_cast<float>(i);
        }

        return score;
    }

    ArtistContainer ConsecutiveArtists::getArtists(db::TrackId trackId)
    {
        using namespace db;

        ArtistContainer res;

        Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        const Track::pointer track{ Track::find(dbSession, trackId) };
        if (!track)
            return res;

        res = track->getArtistIds({});
        std::sort(std::begin(res), std::end(res));

        return res;
    }

} // namespace lms::recommendation::PlaylistGeneratorConstraint
