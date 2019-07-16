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

#include "SimilaritySettings.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "Session.hpp"
#include "TrackFeatures.hpp"

namespace Database {

struct TrackFeatureInfo
{
	std::string	name;
	std::size_t	nbDimensions;
	double		weight;
};

static const std::vector<TrackFeatureInfo> defaultFeatures =
{
	{ "lowlevel.spectral_contrast_coeffs.median",	6,	1. },
	{ "lowlevel.erbbands.median",			40,	1. },
	{ "tonal.hpcp.median",				36,	1. },
	{ "lowlevel.melbands.median",			40,	1. },
	{ "lowlevel.barkbands.median",			27,	1. },
	{ "lowlevel.mfcc.mean",				13,	1. },
	{ "lowlevel.gfcc.mean",				13,	1. },
};

SimilaritySettingsFeature::SimilaritySettingsFeature(Wt::Dbo::ptr<SimilaritySettings> settings, const std::string& name, std::size_t nbDimensions, double weight)
: _name(name),
_nbDimensions(nbDimensions),
_weight(weight),
_settings(settings)
{
}

SimilaritySettingsFeature::pointer
SimilaritySettingsFeature::create(Session& session, Wt::Dbo::ptr<SimilaritySettings> settings, const std::string& name, std::size_t nbDimensions, double weight)
{
	session.checkUniqueLocked();

	SimilaritySettingsFeature::pointer res {session.getDboSession().add(std::make_unique<SimilaritySettingsFeature>(settings, name, nbDimensions, weight))};
	session.getDboSession().flush();

	return res;
}

void
SimilaritySettings::init(Session& session)
{
	session.checkUniqueLocked();

	pointer settings {session.getDboSession().find<SimilaritySettings>()};
	if (settings)
		return;

	settings = session.getDboSession().add(std::make_unique<SimilaritySettings>());
	for (const auto& feature : defaultFeatures)
		SimilaritySettingsFeature::create(session, settings, feature.name, feature.nbDimensions, feature.weight);
}


SimilaritySettings::pointer
SimilaritySettings::get(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().find<SimilaritySettings>();
}

std::vector<Wt::Dbo::ptr<SimilaritySettingsFeature>>
SimilaritySettings::getFeatures() const
{
	return std::vector<Wt::Dbo::ptr<SimilaritySettingsFeature>>(_features.begin(), _features.end());
}

} // namespace Database

