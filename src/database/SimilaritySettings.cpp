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

static const std::map<std::string, double> defaultFeatures =
{
	{ "lowlevel.spectral_contrast_coeffs.median",	1. },
	{ "lowlevel.erbbands.median",			1. },
	{ "tonal.hpcp.median",				1. },
	{ "lowlevel.melbands.median",			1. },
	{ "lowlevel.barkbands.median",			1. },
	{ "lowlevel.mfcc.mean",				1. },
	{ "lowlevel.gfcc.mean",				1. },
};

SimilaritySettingsFeature::SimilaritySettingsFeature(Wt::Dbo::ptr<SimilaritySettings> settings, const std::string& name, double weight)
: _name {name},
_weight {weight},
_settings {settings}
{
}

SimilaritySettingsFeature::pointer
SimilaritySettingsFeature::create(Session& session, Wt::Dbo::ptr<SimilaritySettings> settings, const std::string& name, double weight)
{
	session.checkUniqueLocked();

	SimilaritySettingsFeature::pointer res {session.getDboSession().add(std::make_unique<SimilaritySettingsFeature>(settings, name, weight))};
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
	for (const auto& [feature, weight] : defaultFeatures)
		SimilaritySettingsFeature::create(session, settings, feature, weight);
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

