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

#include "SameReleaseConstraint.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Track.hpp"

namespace lms::recommendation
{
    namespace
    {
        db::ReleaseId getReleaseId(db::IDb& db, db::TrackId trackId)
        {
            db::Session& session{ db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };
            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (!track)
                return {};
            const db::Release::pointer release{ track->getRelease() };
            return release ? release->getId() : db::ReleaseId{};
        }
    } // namespace

    SameReleaseConstraint::SameReleaseConstraint(db::IDb& db, std::size_t window)
        : _db{ db }
        , _window{ window }
    {
    }

    SameReleaseConstraint::~SameReleaseConstraint() = default;

    float SameReleaseConstraint::computeScore(const TrackCandidateContext& context) const
    {
        const db::ReleaseId candidateRelease{ getReleaseId(_db, context.candidateTrackId) };
        if (!candidateRelease.isValid())
            return {};

        float score{};
        const auto& selected{ context.selectedTracks };
        for (std::size_t i{ 1 }; i <= _window && i <= selected.size(); ++i)
        {
            if (getReleaseId(_db, selected[selected.size() - i]) == candidateRelease)
                score += 1.F / static_cast<float>(i);
        }
        return score;
    }
} // namespace lms::recommendation
