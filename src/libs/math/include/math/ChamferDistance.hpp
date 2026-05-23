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

#include <cassert>
#include <concepts>
#include <limits>
#include <ranges>
#include <type_traits>

namespace lms::math
{
    /// Computes the Chamfer distance from set A to set B.
    /// For each element in A, finds the nearest element in B and sums these minimum distances.
    /// The result is normalized by the size of A.
    ///
    /// @tparam DistanceFunc A functor type: constructed with an element of A as ref,
    ///                      then called with each element of B as target.
    /// @param A A forward range of vectors
    /// @param B A forward range of vectors (same element type as A)
    /// @return The normalized sum of minimum distances from A to B
    template<typename DistanceFunc,
             std::ranges::forward_range RangeA,
             std::ranges::forward_range RangeB>
        requires std::same_as<std::ranges::range_value_t<RangeA>,
                              std::ranges::range_value_t<RangeB>>
    auto chamferDistanceAtoB(const RangeA& A, const RangeB& B)
    {
        using Vector = std::ranges::range_value_t<RangeA>;
        using ValueType = std::invoke_result_t<DistanceFunc, const Vector&>;

        assert(!std::ranges::empty(A));
        assert(!std::ranges::empty(B));

        ValueType total{};
        std::size_t countA{};

        for (const auto& a : A)
        {
            DistanceFunc distFunc{ a };
            ValueType bestDist{ std::numeric_limits<ValueType>::max() };

            for (const auto& b : B)
            {
                const ValueType dist{ distFunc(b) };
                if (dist < bestDist)
                    bestDist = dist;
            }

            total += bestDist;
            ++countA;
        }

        return total / static_cast<ValueType>(countA);
    }

    /// Computes the symmetrical Chamfer distance between two sets.
    /// Returns the average of chamferDistanceAtoB(A, B) and chamferDistanceAtoB(B, A).
    ///
    /// @tparam DistanceFunc A functor type: constructed with the ref element,
    ///                      then called with each candidate element.
    /// @param A A forward range of vectors
    /// @param B A forward range of vectors (same element type as A)
    /// @return The symmetrical Chamfer distance
    template<typename DistanceFunc,
             std::ranges::forward_range RangeA,
             std::ranges::forward_range RangeB>
        requires std::same_as<std::ranges::range_value_t<RangeA>,
                              std::ranges::range_value_t<RangeB>>
    auto symmetricalChamferDistance(const RangeA& A, const RangeB& B)
    {
        const auto aToB{ chamferDistanceAtoB<DistanceFunc>(A, B) };
        const auto bToA{ chamferDistanceAtoB<DistanceFunc>(B, A) };
        return (aToB + bToA) / static_cast<decltype(aToB)>(2);
    }
} // namespace lms::math
