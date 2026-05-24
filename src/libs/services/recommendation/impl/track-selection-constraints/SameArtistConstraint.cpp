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

#include "SameArtistConstraint.hpp"

#include <algorithm>
#include <vector>

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"

namespace lms::recommendation
{
    namespace
    {
        std::vector<db::ArtistId> getArtistIds(db::IDb& db, db::TrackId trackId)
        {
            db::Session& session{ db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };
            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (!track)
                return {};

            std::vector<db::ArtistId> ids{ track->getArtistIds({}) }; // all track-level link types

            // Also include album artists from ReleaseArtistLink
            if (const db::Release::pointer release{ track->getRelease() })
            {
                release->visitArtistLinks([&](const db::ReleaseArtistLink::pointer& link) {
                    ids.push_back(link->getArtistId());
                });
            }

            std::sort(ids.begin(), ids.end());
            return ids;
        }

        bool hasCommonArtist(const std::vector<db::ArtistId>& a, const std::vector<db::ArtistId>& b)
        {
            // Both vectors are sorted
            auto ia{ a.cbegin() }, ib{ b.cbegin() };
            while (ia != a.cend() && ib != b.cend())
            {
                if (*ia == *ib)
                    return true;

                if (*ia < *ib)
                    ++ia;
                else
                    ++ib;
            }
            return false;
        }
    } // namespace

    SameArtistConstraint::SameArtistConstraint(db::IDb& db, std::size_t window)
        : _db{ db }
        , _window{ window }
    {
    }

    SameArtistConstraint::~SameArtistConstraint() = default;

    float SameArtistConstraint::computeScore(const TrackCandidateContext& context) const
    {
        const std::vector<db::ArtistId> candidateArtists{ getArtistIds(_db, context.candidateTrackId) };
        if (candidateArtists.empty())
            return {};

        float score{};
        const auto& selected{ context.selectedTracks };
        for (std::size_t i{ 1 }; i <= _window && i <= selected.size(); ++i)
        {
            if (hasCommonArtist(candidateArtists, getArtistIds(_db, selected[selected.size() - i])))
                score += 1.F / static_cast<float>(i);
        }
        return score;
    }
} // namespace lms::recommendation
