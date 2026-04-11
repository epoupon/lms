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

namespace lms::core::math
{
    class StatsAccumulator
    {
    public:
        void add(double x);
        std::size_t getCount() const;
        double getMean() const;

        double getSampleStdDev() const;
        double getSampleVariance() const;
        double getSampleSkewness() const;

        double getPopulationVariance() const;
        double getPopulationStdDev() const;

    private:
        std::size_t n{};
        double mean{};
        double M2{};
        double M3{};
    };
} // namespace lms::core::math