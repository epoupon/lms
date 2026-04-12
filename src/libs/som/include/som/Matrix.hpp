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

#include <cassert>
#include <functional>
#include <vector>

namespace lms::som
{
    using Coordinate = unsigned;

    struct MatrixPosition
    {
        Coordinate x;
        Coordinate y;

        constexpr auto operator<=>(const MatrixPosition& other) const = default;
    };

    // Internally using Row-major order
    template<typename T>
    class Matrix
    {
    public:
        Matrix() = default;

        constexpr Matrix(Coordinate width, Coordinate height)
        {
            resize(width, height);
        }

        constexpr Matrix(Coordinate width, Coordinate height, const T& value)
        {
            resize(width, height, value);
        }

        constexpr void fill(const T& value)
        {
            std::fill(std::begin(_values), std::end(_values), value);
        }

        constexpr void resize(Coordinate width, Coordinate height)
        {
            _width = width;
            _height = height;
            _values.assign(static_cast<std::size_t>(_width) * _height, T{});
        }

        constexpr void resize(Coordinate width, Coordinate height, const T& value)
        {
            _width = width;
            _height = height;
            _values.assign(static_cast<std::size_t>(_width) * _height, value);
        }

        constexpr Coordinate getHeight() const { return _height; }
        constexpr Coordinate getWidth() const { return _width; }

        constexpr T& get(Coordinate x, Coordinate y)
        {
            assert(x < _width);
            assert(y < _height);
            return _values[static_cast<std::size_t>(x) + static_cast<std::size_t>(_width) * y];
        }

        constexpr T& get(const MatrixPosition& position)
        {
            return get(position.x, position.y);
        }

        constexpr const T& get(Coordinate x, Coordinate y) const
        {
            assert(x < _width);
            assert(y < _height);
            return _values[static_cast<std::size_t>(x) + static_cast<std::size_t>(_width) * y];
        }

        constexpr const T& get(const MatrixPosition& position) const
        {
            return get(position.x, position.y);
        }

        constexpr T& operator[](const MatrixPosition& position) { return get(position); }
        constexpr const T& operator[](const MatrixPosition& position) const { return get(position); }

        // Best score means closest to 0
        template<typename Distance>
        MatrixPosition getPositionMinDistance(Distance distFunc) const
        {
            assert(!_values.empty());
            std::size_t bestIndex{};

            auto minDist{ distFunc(_values[0]) };

            const std::size_t size{ _values.size() };

            for (std::size_t i{ 1 }; i < size; ++i)
            {
                const auto s{ distFunc(_values[i]) };
                if (s < minDist)
                {
                    minDist = s;
                    bestIndex = i;
                }
            }
            return MatrixPosition{ static_cast<Coordinate>(bestIndex % _width), static_cast<Coordinate>(bestIndex / _width) };
        }

    private:
        Coordinate _width{};
        Coordinate _height{};
        std::vector<T> _values;
    };
} // namespace lms::som

namespace std
{
    template<>
    class hash<lms::som::MatrixPosition>
    {
    public:
        size_t operator()(const lms::som::MatrixPosition& s) const
        {
            size_t h1 = std::hash<lms::som::Coordinate>()(s.x);
            size_t h2 = std::hash<lms::som::Coordinate>()(s.y);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
} // namespace std
