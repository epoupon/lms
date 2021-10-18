/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Recommendation {

using FeatureName = std::string;
using FeatureNames = std::unordered_set<FeatureName>;
using FeatureValue = double;
using FeatureValues = std::vector<FeatureValue>;
using FeatureValuesMap = std::unordered_map<FeatureName, FeatureValues>;

struct FeatureDef
{
	std::size_t nbDimensions {};
};

FeatureDef getFeatureDef(const FeatureName& featureName);
FeatureNames getFeatureNames();

struct FeatureSettings
{
	double weight {};
};
using FeatureSettingsMap = std::unordered_map<FeatureName, FeatureSettings>;

} // namespace Recommendation
