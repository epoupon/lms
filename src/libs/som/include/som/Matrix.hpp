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

namespace SOM
{

using Coordinate = unsigned;
using Norm = InputVector::value_type;

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

template <typename T>
class Matrix
{
	public:
		Matrix() = default;

		Matrix(Coordinate width, Coordinate height)
		: _width {width}
		, _height {height}
		{
			_values.resize(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height));
		}

		template<typename... CtArgs>
		Matrix(Coordinate width, Coordinate height, CtArgs... args)
		: _width {width}
		, _height {height}
		{
			 _values.resize(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height), T{args...});
		}

		void clear()
		{
			std::vector<T> values(static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height));
			_values.swap(values);
		}

		Coordinate getHeight() const { return _height; }
		Coordinate getWidth() const { return _width; }

		T& get(const Position& position)
		{
			assert(position.x < _width);
			assert(position.y < _height);
			return _values[position.x + _width*position.y];
		}

		const T& get(const Position& position) const
		{
			assert(position.x < _width);
			assert(position.y < _height);
			return _values[position.x + _width*position.y];
		}

		T& operator[](const Position& position) { return get(position); }
		const T& operator[](const Position& position) const { return get(position); }

		template <typename Func>
		Position getPositionMinElement(Func func) const
		{
			assert(!_values.empty());

			auto it {std::min_element(_values.begin(), _values.end(), std::move(func))};
			auto index {static_cast<Coordinate>(std::distance(_values.begin(), it))};

			return {index % _height, index / _height};
		}

	private:
		Coordinate		_width {};
		Coordinate		_height {};
		std::vector<T>	_values;
};

} // ns SOM

namespace std {

template<>
class hash<SOM::Position>
{
	public:
		size_t operator()(const SOM::Position& s) const
		{
			size_t h1 = std::hash<SOM::Coordinate>()(s.x);
			size_t h2 = std::hash<SOM::Coordinate>()(s.y);
			return h1 ^ (h2 << 1);
		}
};

} // ns std

