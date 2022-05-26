
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

#include <vector>
#include <cmath>

#include "utils/Exception.hpp"

namespace SOM
{

class Exception : public LmsException
{
	public:
		using LmsException::LmsException;
};

class InputVector
{
	public:
		using value_type = double;
		using Norm = double;
		using Distance = double;

		InputVector(std::size_t nbDimensions, value_type defaultValue = value_type {}) : _values(nbDimensions, defaultValue) {}

		bool hasSameDimension(const InputVector& other) const
		{
			return _values.size() == other._values.size();
		}

		std::size_t getNbDimensions() const
		{
			return _values.size();
		}

		value_type& operator[](std::size_t index)
		{
			if (index >= getNbDimensions())
				throw Exception("Bad range");

			return _values[index];
		}

		value_type operator[](std::size_t index) const
		{
			if (index >= getNbDimensions())
				throw Exception("Bad range");

			return _values[index];
		}

		InputVector& operator+=(const InputVector& other)
		{
			if (!hasSameDimension(other.getNbDimensions()))
				throw Exception {"Not the same dimension count"};

			for (std::size_t i {}; i < _values.size(); ++i)
			{
				_values[i] += other[i];
			}

			return *this;
		}

		InputVector& operator-=(const InputVector& other)
		{
			if (!hasSameDimension(other.getNbDimensions()))
				throw Exception {"Not the same dimension count"};

			for (std::size_t i {}; i < _values.size(); ++i)
			{
				_values[i] -= other[i];
			}

			return *this;
		}

		InputVector& operator*=(value_type factor)
		{
			for (std::size_t i {}; i < _values.size(); ++i)
			{
				_values[i] *= factor;
			}

			return *this;
		}

		Norm computeNorm() const
		{
			Norm res {};
			for (value_type val : _values)
				res += val * val;
			return std::sqrt(res);
		}

		Distance computeEuclidianSquareDistance(const InputVector& other, const InputVector& weights) const
		{
			if (!hasSameDimension(other.getNbDimensions())
				|| !hasSameDimension(weights.getNbDimensions()))
			{
				throw Exception {"Not the same dimension count"};
			}

			Distance res {};

			for (std::size_t i {}; i < getNbDimensions(); ++i)
			{
				const InputVector::value_type diff {_values[i] - other._values[i]};
				res += diff * diff * weights._values[i];
			}

			return res;
		}

		std::vector<value_type>::iterator begin()
		{
			return _values.begin();
		}

		std::vector<value_type>::const_iterator begin() const
		{
			return _values.cbegin();
		}

		std::vector<value_type>::const_iterator cbegin() const
		{
			return _values.cbegin();
		}

		std::vector<value_type>::iterator end()
		{
			return _values.end();
		}

		std::vector<value_type>::const_iterator end() const
		{
			return _values.cend();
		}

		std::vector<value_type>::const_iterator cend() const
		{
			return _values.cend();
		}

	private:
		friend class InputVector operator-(const InputVector& a, const InputVector& b)
		{
			if (!a.hasSameDimension(b.getNbDimensions()))
				throw Exception {"Not the same dimension count"};

			InputVector res {a.getNbDimensions()};

			for (std::size_t i {}; i < res._values.size(); ++i)
				res._values[i] = a._values[i] - b._values[i];

			return res;
		}

		friend std::ostream&
		operator<<(std::ostream& os, const InputVector& a)
		{
			os << "[";
			for (value_type val : a._values)
			{
				os << val << " ";
			}
			os << "]";

			return os;
		}

		std::vector<value_type> _values;
};

}
