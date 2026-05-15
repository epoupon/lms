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
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

#include "math/EuclideanDistance.hpp"

namespace lms::math
{
    template<typename VectorType>
    class MedoidCalculator
    {
    public:
        static_assert(!std::is_const_v<VectorType>);

        using value_type = typename VectorType::value_type;
        using size_type = std::size_t;

        // Add a single vector, returning its index
        size_type add(const VectorType& value)
        {
            _vectors.push_back(value);
            return _vectors.size() - 1;
        }

        // Compute the medoid: the vector with minimum sum of squared distances to all others
        // Returns the index of the medoid in the added vectors
        size_type findMedoidIndex() const
        {
            assert(!empty());

            size_type medoidIndex{};
            value_type minTotalDistance{ std::numeric_limits<value_type>::max() };

            for (size_type i{}; i < _vectors.size(); ++i)
            {
                value_type totalDistance{};
                const SquaredEuclideanDistance distFunc{ _vectors[i] };
                for (size_type j{}; j < _vectors.size(); ++j)
                {
                    if (i != j)
                        totalDistance += distFunc(_vectors[j]);
                }

                if (totalDistance < minTotalDistance)
                {
                    minTotalDistance = totalDistance;
                    medoidIndex = i;
                }
            }

            return medoidIndex;
        }

        // Compute the medoid vector itself
        VectorType finalize() const
        {
            return _vectors[findMedoidIndex()];
        }

        // Get a specific vector by index
        const VectorType& getVector(size_type index) const
        {
            assert(index < _vectors.size());
            return _vectors[index];
        }

        // Query methods
        bool empty() const
        {
            return _vectors.empty();
        }

        size_type count() const
        {
            return _vectors.size();
        }

        void clear()
        {
            _vectors.clear();
        }

    private:
        std::vector<VectorType> _vectors;
    };

    template<typename VectorType>
    VectorType computeMedoid(std::span<const VectorType> values)
    {
        MedoidCalculator<VectorType> calculator;
        for (const auto& value : values)
            calculator.add(value);
        return calculator.finalize();
    }

    template<typename VectorType>
    VectorType computeNormalizedMedoid(std::span<const VectorType> values)
    {
        MedoidCalculator<VectorType> calculator;
        for (const auto& value : values)
            calculator.add(value);
        return calculator.finalizeNormalized();
    }
} // namespace lms::math
