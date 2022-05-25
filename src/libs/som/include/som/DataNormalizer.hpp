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
#include <ostream>

#include "Network.hpp"

namespace SOM
{

class DataNormalizer
{
	public:
		struct MinMax
		{
			InputVector::value_type min;
			InputVector::value_type max;
		};

		DataNormalizer(std::size_t inputDimCount);

		std::size_t getInputDimCount() const { return _inputDimCount; }
		const MinMax& getValue(std::size_t index) const;

		void setValue(std::size_t index, const MinMax& minMax);

		void computeNormalizationFactors(const std::vector<InputVector>& dataSamples);

		void normalizeData(InputVector& data) const;

		void dump(std::ostream& os) const;

	private:
		InputVector::value_type normalizeValue(InputVector::value_type value, std::size_t dimensionId) const;

		const std::size_t _inputDimCount;

		std::vector<MinMax> _minmax; // Indexed min/max used to normalize data
};

} // namespace SOM
