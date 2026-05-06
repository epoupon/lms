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
#include <span>
#include <type_traits>

namespace lms::math
{
    template<typename VectorType>
    class CentroidCalculator
    {
    public:
        static_assert(!std::is_const_v<VectorType>);

        using value_type = typename VectorType::value_type;
        using size_type = std::size_t;

        constexpr void add(const VectorType& value)
        {
            _sum += value;
            ++_count;
        }

        template<typename InputIt>
        constexpr void add(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
                add(*first);
        }

        constexpr VectorType finalize() const
        {
            assert(_count > 0);
            VectorType result{ _sum };
            result *= static_cast<value_type>(1) / static_cast<value_type>(_count);
            return result;
        }

        constexpr VectorType finalizeNormalized() const
        {
            VectorType result{ finalize() };
            result.normalizeL2();
            return result;
        }

        constexpr bool empty() const
        {
            return _count == 0;
        }

        constexpr size_type count() const
        {
            return _count;
        }

    private:
        VectorType _sum;
        size_type _count{};
    };

    template<typename VectorType>
    constexpr VectorType computeCentroid(std::span<const VectorType> values)
    {
        CentroidCalculator<VectorType> calculator;
        calculator.add(std::cbegin(values), std::cend(values));
        return calculator.finalize();
    }

    template<typename VectorType>
    constexpr VectorType computeNormalizedCentroid(std::span<const VectorType> values)
    {
        CentroidCalculator<VectorType> calculator;
        calculator.add(std::cbegin(values), std::cend(values));
        return calculator.finalizeNormalized();
    }
} // namespace lms::math
