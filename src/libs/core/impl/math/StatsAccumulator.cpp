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

#include "core/math/StatsAccumulator.hpp"

#include <cmath>

namespace lms::core::math
{
    void StatsAccumulator::add(double x)
    {
        const double n1{ static_cast<double>(n++) };
        const double nn{ static_cast<double>(n) };

        const double delta{ x - mean };
        const double delta_n{ delta / nn };
        const double term1{ delta * delta_n * n1 };

        mean += delta_n;

        M3 += term1 * delta_n * (nn - 2) - 3.0 * delta_n * M2;
        M2 += term1;
    }

    std::size_t StatsAccumulator::getCount() const
    {
        return n;
    }

    double StatsAccumulator::getMean() const
    {
        return mean;
    }

    double StatsAccumulator::getSampleVariance() const
    {
        if (n < 2)
            return 0.0;

        return M2 / (n - 1);
    }

    double StatsAccumulator::getPopulationVariance() const
    {
        if (n < 1)
            return 0.0;

        return M2 / n;
    }

    double StatsAccumulator::getSampleStdDev() const
    {
        return std::sqrt(getSampleVariance());
    }

    double StatsAccumulator::getPopulationStdDev() const
    {
        return std::sqrt(getPopulationVariance());
    }

    double StatsAccumulator::getSampleSkewness() const
    {
        if (n < 3 || M2 == 0.0)
            return 0.0;

        const double nn{ static_cast<double>(n) };
        return (std::sqrt(nn * (nn - 1)) / (nn - 2)) * (M3 / (M2 * std::sqrt(M2)));
    }
} // namespace lms::core::math