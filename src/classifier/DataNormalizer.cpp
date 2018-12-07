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

#include "DataNormalizer.hpp"

#include <iostream>

#include <algorithm>

namespace SOM
{

DataNormalizer::DataNormalizer(std::size_t inputDimCount)
: _inputDimCount(inputDimCount)
{
}


void
DataNormalizer::computeNormalizationFactors(const std::vector<InputVector>& inputVectors)
{
	// For each dimension of the input, compute the min/max
	_minmax.clear();
	_minmax.resize(_inputDimCount);

	for (std::size_t dimId = 0; dimId < _inputDimCount; ++dimId)
	{
		std::vector<InputVector::value_type> values;

		for (const auto& inputVector: inputVectors)
		{
			checkSameDimensions(inputVector, _inputDimCount);
			values.push_back(inputVector[dimId]);
		}

		auto result = std::minmax_element(values.begin(), values.end());
		_minmax[dimId] = {*result.first, *result.second};
	}
}

void
DataNormalizer::normalizeData(InputVector& a) const
{
	checkSameDimensions(a, _inputDimCount);

	for (std::size_t dimId = 0; dimId < _inputDimCount; ++dimId)
	{
		// clamp
		if (a[dimId] > _minmax[dimId].max)
			a[dimId] = _minmax[dimId].max;
		else if (a[dimId] < _minmax[dimId].min)
			a[dimId] = _minmax[dimId].min;

		a[dimId] = (a[dimId] - _minmax[dimId].min) / (_minmax[dimId].max - _minmax[dimId].min);
	}
}

} // namespace SOM
