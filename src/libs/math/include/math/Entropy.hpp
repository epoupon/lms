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
#include <span>

namespace lms::math
{
    template<typename FloatType = float>
    FloatType entropy(std::span<const FloatType> c)
    {
        FloatType sum{};

        for (auto v : c)
            sum += v;

        constexpr FloatType epsilon{ 1e-12 };

        if (sum <= epsilon)
            return {};

        const FloatType invSum{ FloatType{ 1 } / sum };

        FloatType res{};
        for (auto v : c)
        {
            if (v <= epsilon)
                continue;

            const FloatType p{ v * invSum };
            res -= p * std::log(p);
        }

        return res;
    }
} // namespace lms::math