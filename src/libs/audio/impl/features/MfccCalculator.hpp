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

#include <array>
#include <cmath>
#include <numbers>
#include <span>

namespace lms::audio::features
{
    template<std::size_t melBandCount, std::size_t mfccBandCount, typename FloatType = float>
    class MfccCalculator
    {
    public:
        using Input = std::span<const FloatType, melBandCount>;
        using Output = std::array<FloatType, mfccBandCount>;

        [[nodiscard]] Output apply(Input logMelBands) const
        {
            Output result{};

            for (std::size_t k{}; k < mfccBandCount; ++k)
            {
                FloatType sum{};
                for (std::size_t m{}; m < melBandCount; ++m)
                    sum += logMelBands[m] * dctTable[k][m];

                result[k] = sum;
            }

            return result;
        }

    private:
        static constexpr auto makeDctTable()
        {
            std::array<std::array<FloatType, melBandCount>, mfccBandCount> table{};

            constexpr FloatType factor{ std::numbers::pi_v<FloatType> / FloatType{ melBandCount } };
            constexpr FloatType baseScale{ std::sqrt(FloatType{ 2 } / FloatType{ melBandCount }) };

            for (std::size_t k{}; k < mfccBandCount; ++k)
            {
                FloatType scale = baseScale;

                if (k == 0)
                    scale *= std::sqrt(FloatType{ 0.5 });

                for (std::size_t m{}; m < melBandCount; ++m)
                {
                    table[k][m] = scale * std::cos(factor * (static_cast<FloatType>(m) + FloatType{ 0.5 }) * static_cast<FloatType>(k));
                }
            }

            return table;
        }

        static inline constexpr auto dctTable{ makeDctTable() };
    };
} // namespace lms::audio::features