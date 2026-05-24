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

#include "TrackCandidateContext.hpp"

namespace lms::recommendation
{
    namespace
    {
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

    SameArtistConstraint::SameArtistConstraint(const TrackMetadataMap& trackMetadata, std::size_t window)
        : _trackMetadata{ trackMetadata }
        , _window{ window }
    {
    }

    SameArtistConstraint::~SameArtistConstraint() = default;

    float SameArtistConstraint::computeScore(const TrackCandidateContext& context) const
    {
        const auto it{ _trackMetadata.find(context.candidateTrackId) };
        if (it == _trackMetadata.cend() || it->second.artistIds.empty())
            return {};

        const auto& candidateArtists{ it->second.artistIds };

        float score{};
        const auto& selected{ context.selectedTracks };
        for (std::size_t i{ 1 }; i <= _window && i <= selected.size(); ++i)
        {
            const auto sit{ _trackMetadata.find(selected[selected.size() - i]) };
            if (sit != _trackMetadata.cend() && hasCommonArtist(candidateArtists, sit->second.artistIds))
                score += 1.F / static_cast<float>(i);
        }
        return score;
    }
} // namespace lms::recommendation
