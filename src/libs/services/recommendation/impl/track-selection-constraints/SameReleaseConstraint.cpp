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

#include "TrackCandidateContext.hpp"

namespace lms::recommendation
{
    SameReleaseConstraint::SameReleaseConstraint(const TrackMetadataMap& trackMetadata, std::size_t window)
        : _trackMetadata{ trackMetadata }
        , _window{ window }
    {
    }

    SameReleaseConstraint::~SameReleaseConstraint() = default;

    float SameReleaseConstraint::computeScore(const TrackCandidateContext& context) const
    {
        const auto it{ _trackMetadata.find(context.candidateTrackId) };
        if (it == _trackMetadata.cend() || !it->second.releaseId.isValid())
            return {};

        const db::ReleaseId candidateRelease{ it->second.releaseId };

        float score{};
        const auto& selected{ context.selectedTracks };
        for (std::size_t i{ 1 }; i <= _window && i <= selected.size(); ++i)
        {
            const auto itMetadata{ _trackMetadata.find(selected[selected.size() - i]) };
            if (itMetadata != _trackMetadata.cend() && itMetadata->second.releaseId == candidateRelease)
                score += 1.F / static_cast<float>(i);
        }
        return score;
    }
} // namespace lms::recommendation
