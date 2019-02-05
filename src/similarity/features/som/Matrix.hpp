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
#include <sstream>
#include <vector>

namespace SOM
{

struct Coords
{
	std::size_t x;
	std::size_t y;

	bool operator<(const Coords& other) const
	{
		if (x == other.x)
			return y < other.y;
		else
			return x < other.x;
	}

	bool operator==(const Coords& other) const
	{
		return x == other.x && y == other.y;
	}
};

template <typename T>
class Matrix
{
	public:

		Matrix() = default;

		Matrix(std::size_t width, std::size_t height)
		: _width(width),
		_height(height)
		{
			_values.resize(_width*_height);
		}

		Matrix(std::size_t width, std::size_t height, std::vector<T> values)
		: _width(width),
		_height(height),
		_values(std::move(values))
		{
			assert(_values.size() == _width * _height);
		}

		void clear()
		{
			std::vector<T> values(_width*_height);
			_values.swap(values);
		}

		std::size_t getHeight() const { return _height; }
		std::size_t getWidth() const { return _width; }

		T& get(Coords coords)
		{
			assert(coords.x < _width);
			assert(coords.y < _height);
			return _values[coords.x + _width*coords.y];
		}

		const T& get(Coords coords) const
		{
			assert(coords.x < _width);
			assert(coords.y < _height);
			return _values[coords.x + _width*coords.y];
		}

		T& operator[](Coords coords) { return get(coords); }
		const T& operator[](Coords coords) const { return get(coords); }

		template <typename Func>
		Coords getCoordsMinElement(Func func) const
		{
			assert(!_values.empty());

			auto it = std::min_element(_values.begin(), _values.end(), func);
			auto index = std::distance(_values.begin(), it);

			return {index % _height, index / _height};
		}

	private:

		std::size_t _width = 0;
		std::size_t _height = 0;
		std::vector<T> _values;
};

} // ns SOM
