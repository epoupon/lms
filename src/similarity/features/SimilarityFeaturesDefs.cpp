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

#include "SimilarityFeaturesDefs.hpp"

#include <unordered_map>
#include "utils/Exception.hpp"

namespace Similarity {

static const std::unordered_map<FeatureName, FeatureDef> featureDefinitions
{
	{ "lowlevel.spectral_contrast_coeffs.median",	{6}},
	{ "lowlevel.erbbands.median",			{40}},
	{ "tonal.hpcp.median",				{36}},
	{ "lowlevel.melbands.median",			{40}},
	{ "lowlevel.barkbands.median",			{27}},
	{ "lowlevel.mfcc.mean",				{13}},
	{ "lowlevel.gfcc.mean",				{13}},
};

FeatureDef
getFeatureDef(const FeatureName& featureName)
{
	auto it {featureDefinitions.find(featureName)};
	if (it == std::cend(featureDefinitions))
		throw LmsException {"Unhandled requested feature '" + featureName + "'"};

	return it->second;
}

} // namespace Similarity

