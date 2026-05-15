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
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>

namespace lms::math
{
    template<std::size_t FrameSize, typename FloatType = float>
    class HannWindow
    {
        static_assert(FrameSize > 0, "FrameSize must be greater than zero");

    public:
        HannWindow()
        {
            if constexpr (FrameSize == 1)
            {
                _coefficients[0] = FloatType{ 1 };
                _energy = FloatType{ 1 };
                return;
            }

            constexpr auto frameSize{ static_cast<FloatType>(FrameSize) };
            for (std::size_t i{}; i < FrameSize; ++i)
            {
                const auto coefficient{ static_cast<FloatType>(0.5) * (FloatType{ 1 } - std::cos(static_cast<FloatType>(2) * std::numbers::pi_v<FloatType> * static_cast<FloatType>(i) / (frameSize - FloatType{ 1 }))) };
                _coefficients[i] = coefficient;
                _energy += coefficient * coefficient;
            }
        }

        [[nodiscard]] std::span<const FloatType, FrameSize> values() const noexcept { return _coefficients; }

        [[nodiscard]] FloatType energy() const noexcept { return _energy; }

        void apply(std::span<const FloatType, FrameSize> input, std::span<FloatType, FrameSize> output) const noexcept
        {
            assert(input.size() == FrameSize);
            assert(output.size() == FrameSize);

            for (std::size_t i{}; i < FrameSize; ++i)
                output[i] = input[i] * _coefficients[i];
        }

    private:
        std::array<FloatType, FrameSize> _coefficients{};
        FloatType _energy{};
    };
} // namespace lms::math