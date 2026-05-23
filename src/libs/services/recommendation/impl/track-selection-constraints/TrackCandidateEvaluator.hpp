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

#pragma once

#include <memory>
#include <vector>

#include "ITrackCandidateHardConstraint.hpp"
#include "ITrackCandidateSoftConstraint.hpp"

namespace lms::recommendation
{
    class TrackCandidateEvaluator
    {
    public:
        struct WeightedSoftConstraint
        {
            std::unique_ptr<ITrackCandidateSoftConstraint> constraint;
            float weight{ 1.0F };
        };

        void addSoftConstraint(std::unique_ptr<ITrackCandidateSoftConstraint> constraint, float weight = 1.0F)
        {
            _softConstraints.emplace_back(WeightedSoftConstraint{ .constraint = std::move(constraint), .weight = weight });
        }

        void addHardConstraint(std::unique_ptr<ITrackCandidateHardConstraint> constraint)
        {
            _hardConstraints.emplace_back(std::move(constraint));
        }

        bool rejects(const TrackCandidateContext& context) const
        {
            for (const auto& c : _hardConstraints)
            {
                if (c->rejects(context))
                    return true;
            }
            return false;
        }

        float score(const TrackCandidateContext& context) const
        {
            float total{};
            for (const auto& item : _softConstraints)
                total += item.weight * item.constraint->computeScore(context);
            return total;
        }

    private:
        std::vector<WeightedSoftConstraint> _softConstraints;
        std::vector<std::unique_ptr<ITrackCandidateHardConstraint>> _hardConstraints;
    };
} // namespace lms::recommendation
