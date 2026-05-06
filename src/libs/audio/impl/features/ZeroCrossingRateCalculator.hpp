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

#include <cstddef>
#include <span>

namespace lms::audio::features
{
    template<typename FloatType = float>
    class ZeroCrossingRateCalculator
    {
    public:
        using Input = std::span<const FloatType>;

        [[nodiscard]] FloatType apply(Input samples) const noexcept
        {
            if (samples.size() < 2)
                return FloatType{};

            std::size_t zeroCrossingCount{};
            bool previousNonNegative{ samples[0] >= FloatType{} };

            for (std::size_t i{ 1 }; i < samples.size(); ++i)
            {
                const bool currentNonNegative{ samples[i] >= FloatType{} };
                if (currentNonNegative != previousNonNegative)
                    ++zeroCrossingCount;

                previousNonNegative = currentNonNegative;
            }

            return static_cast<FloatType>(zeroCrossingCount) / static_cast<FloatType>(samples.size());
        }
    };
} // namespace lms::audio::features
