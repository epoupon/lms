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

#include "som/DataNormalizer.hpp"

#include <algorithm>
#include <numeric>
#include <sstream>

namespace SOM
{

template<typename T>
static
T
variance(const std::vector<T>& vec)
{
	std::size_t size {vec.size()};

	if (size == 1)
		return T {};

	const T mean {std::accumulate(vec.begin(), vec.end(), T{}) / size};

	return std::accumulate(vec.begin(), vec.end(), T {},
		[mean, size] (T accumulator, const T& val)
		{
			return accumulator + ((val - mean) * (val - mean) / (size - 1));
		});
}

DataNormalizer::DataNormalizer(std::size_t inputDimCount)
: _inputDimCount{inputDimCount}
{
}

const DataNormalizer::MinMax&
DataNormalizer::getValue(std::size_t index) const
{
	return _minmax[index];
}

void
DataNormalizer::setValue(std::size_t index, const MinMax& minMax)
{
	_minmax[index] = minMax;
}

void
DataNormalizer::computeNormalizationFactors(const std::vector<InputVector>& inputVectors)
{
	if (inputVectors.empty())
		throw Exception("Empty input vectors");

	// For each dimension of the input, compute the min/max
	_minmax.clear();
	_minmax.resize(_inputDimCount);

	for (std::size_t dimId {}; dimId < _inputDimCount; ++dimId)
	{
		std::vector<InputVector::value_type> values;

		for (const auto& inputVector: inputVectors)
		{
			checkSameDimensions(inputVector, _inputDimCount);
			values.push_back(inputVector[dimId]);
		}

		auto result {std::minmax_element(values.begin(), values.end())};
		_minmax[dimId] = {*result.first, *result.second};
	}
}

InputVector::value_type
DataNormalizer::normalizeValue(InputVector::value_type value, std::size_t dimId) const
{
	// clamp
	if (value > _minmax[dimId].max)
		value = _minmax[dimId].max;
	else if (value < _minmax[dimId].min)
		value = _minmax[dimId].min;

	return (value - _minmax[dimId].min) / (_minmax[dimId].max - _minmax[dimId].min);
}

void
DataNormalizer::normalizeData(InputVector& a) const
{
	checkSameDimensions(a, _inputDimCount);

	for (std::size_t dimId {}; dimId < _inputDimCount; ++dimId)
	{
		a[dimId] = normalizeValue(a[dimId], dimId);
	}
}

void
DataNormalizer::dump(std::ostream& os) const
{
	for (std::size_t i {}; i < _inputDimCount; ++i)
		os << "(" << _minmax[i].min << ", " << _minmax[i].max << ")";
}

} // namespace SOM
