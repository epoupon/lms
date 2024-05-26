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

#include <algorithm>
#include <cassert>
#include <functional>
#include <vector>

namespace lms::som
{
    using Coordinate = unsigned;

    struct Position
    {
        Coordinate x;
        Coordinate y;

        bool operator<(const Position& other) const
        {
            if (x == other.x)
                return y < other.y;
            else
                return x < other.x;
        }

        bool operator==(const Position& other) const
        {
            return x == other.x && y == other.y;
        }
    };

    template<typename T>
    class Matrix
    {
    public:
        Matrix() = default;

        Matrix(Coordinate width, Coordinate height)
            : _width{ width }
            , _height{ height }
        {
            _values.resize(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height));
        }

        template<typename... CtrArgs>
        Matrix(Coordinate width, Coordinate height, CtrArgs&&... args)
            : _width{ width }
            , _height{ height }
        {
            _values.resize(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height), T{ std::forward<CtrArgs>(args)... });
        }

        void clear()
        {
            _values.clear();
        }

        Coordinate getHeight() const { return _height; }
        Coordinate getWidth() const { return _width; }

        T& get(const Position& position)
        {
            assert(position.x < _width);
            assert(position.y < _height);
            return _values[position.x + _width * position.y];
        }

        const T& get(const Position& position) const
        {
            assert(position.x < _width);
            assert(position.y < _height);
            return _values[position.x + _width * position.y];
        }

        T& operator[](const Position& position) { return get(position); }
        const T& operator[](const Position& position) const { return get(position); }

        template<typename Func>
        Position getPositionMinElement(Func func) const
        {
            assert(!_values.empty());

            const auto it{ std::min_element(_values.begin(), _values.end(), std::move(func)) };
            const auto index{ static_cast<Coordinate>(std::distance(_values.begin(), it)) };

            return Position{ index % _height, index / _height };
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
    class hash<lms::som::Position>
    {
    public:
        size_t operator()(const lms::som::Position& s) const
        {
            size_t h1 = std::hash<lms::som::Coordinate>()(s.x);
            size_t h2 = std::hash<lms::som::Coordinate>()(s.y);
            return h1 ^ (h2 << 1);
        }
    };
} // namespace std
