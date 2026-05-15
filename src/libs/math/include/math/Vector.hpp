/*
 * Copyright (C) 2018 Emeric Poupon
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

namespace lms::math
{
    template<std::size_t Size, typename FloatType = float>
    class Vector
    {
    public:
        static_assert(std::is_floating_point_v<FloatType>);

        using value_type = FloatType;
        using Norm = FloatType;
        using Distance = FloatType;

        constexpr explicit Vector(value_type initValue = value_type{})
        {
            _values.fill(initValue);
        }

        template<typename... Args>
            requires(sizeof...(Args) == Size) && (std::convertible_to<Args, value_type> && ...)
        constexpr Vector(Args... args)
            : _values{ static_cast<value_type>(args)... }
        {
        }

        constexpr static std::size_t getSize() { return Size; }

        constexpr value_type* data() { return _values.data(); }
        constexpr const value_type* data() const { return _values.data(); }

        constexpr value_type& operator[](std::size_t index) { return _values[index]; }
        constexpr value_type operator[](std::size_t index) const { return _values[index]; }

        constexpr Vector& operator+=(const Vector& other)
        {
            for (std::size_t i{}; i < Size; ++i)
                _values[i] += other[i];

            return *this;
        }

        constexpr Vector& operator-=(const Vector& other)
        {
            for (std::size_t i{}; i < Size; ++i)
                _values[i] -= other[i];

            return *this;
        }

        constexpr Vector& operator*=(value_type factor)
        {
            for (std::size_t i{}; i < Size; ++i)
                _values[i] *= factor;

            return *this;
        }

        Norm computeNorm() const
        {
            Norm res{};
            for (value_type val : _values)
                res += val * val;
            return std::sqrt(res);
        }

        void normalizeL2()
        {
            constexpr value_type smallEpsilon{ 1e-12 };

            const Norm n{ computeNorm() };
            if (n > smallEpsilon)
            {
                for (value_type& v : _values)
                    v /= n;
            }
        }

        auto begin() { return std::begin(_values); }
        auto begin() const { return std::begin(_values); }
        auto cbegin() const { return std::cbegin(_values); }
        auto end() { return std::end(_values); }
        auto end() const { return std::end(_values); }
        auto cend() const { return std::cend(_values); }

    private:
        std::array<value_type, Size> _values;
    };

    template<std::size_t Size, typename FloatType>
    constexpr Vector<Size, FloatType> operator+(const Vector<Size, FloatType>& a, const Vector<Size, FloatType>& b)
    {
        Vector<Size, FloatType> res{ a };
        res += b;
        return res;
    }

    template<std::size_t Size, typename FloatType>
    constexpr Vector<Size, FloatType> operator-(const Vector<Size, FloatType>& a, const Vector<Size, FloatType>& b)
    {
        Vector<Size, FloatType> res{ a };
        res -= b;
        return res;
    }

    template<std::size_t Size, typename FloatType>
    constexpr Vector<Size, FloatType> operator*(const Vector<Size, FloatType>& v, typename Vector<Size, FloatType>::value_type scalar)
    {
        Vector<Size, FloatType> res{ v };
        res *= scalar;
        return res;
    }

    template<std::size_t Size, typename FloatType>
    constexpr Vector<Size, FloatType> operator*(typename Vector<Size, FloatType>::value_type scalar, const Vector<Size, FloatType>& v)
    {
        return v * scalar;
    }

} // namespace lms::math
