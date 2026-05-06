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

#include <cmath>
#include <cstddef>

namespace lms::math
{
    template<typename FloatType = float>
    class StatsAccumulator
    {
    public:
        constexpr void add(FloatType x);
        constexpr std::size_t getCount() const;
        constexpr FloatType getMean() const;

        constexpr FloatType getSampleStdDev() const;
        constexpr FloatType getSampleVariance() const;

        constexpr FloatType getPopulationVariance() const;
        constexpr FloatType getPopulationStdDev() const;

    private:
        std::size_t n{};
        double mean{};
        double M2{};
    };

    template<typename FloatType>
    inline constexpr void StatsAccumulator<FloatType>::add(FloatType x)
    {
        const double n1{ static_cast<double>(n++) };
        const double nn{ static_cast<double>(n) };

        const double delta{ static_cast<double>(x) - mean };
        const double delta_n{ delta / nn };
        const double term1{ delta * delta_n * n1 };

        mean += delta_n;
        M2 += term1;
    }

    template<typename FloatType>
    inline constexpr std::size_t StatsAccumulator<FloatType>::getCount() const
    {
        return n;
    }

    template<typename FloatType>
    inline constexpr FloatType StatsAccumulator<FloatType>::getMean() const
    {
        return static_cast<FloatType>(mean);
    }

    template<typename FloatType>
    inline constexpr FloatType StatsAccumulator<FloatType>::getSampleVariance() const
    {
        if (n < 2)
            return FloatType{};

        return static_cast<FloatType>(M2 / (n - 1));
    }

    template<typename FloatType>
    inline constexpr FloatType StatsAccumulator<FloatType>::getPopulationVariance() const
    {
        if (n < 1)
            return FloatType{};

        return static_cast<FloatType>(M2 / n);
    }

    template<typename FloatType>
    inline constexpr FloatType StatsAccumulator<FloatType>::getSampleStdDev() const
    {
        return static_cast<FloatType>(std::sqrt(getSampleVariance()));
    }

    template<typename FloatType>
    inline constexpr FloatType StatsAccumulator<FloatType>::getPopulationStdDev() const
    {
        return static_cast<FloatType>(std::sqrt(getPopulationVariance()));
    }
} // namespace lms::math
