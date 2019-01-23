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

#include "TrackFeature.hpp"

namespace {

std::set<std::string> defaultTrackFeaturesNames =
{
	"lowlevel.average_loudness",
	"lowlevel.barkbands_flatness_db.mean",
	"lowlevel.dissonance.mean",
	"lowlevel.dynamic_complexity",
	"lowlevel.hfc.mean",				// GOOD
	"lowlevel.melbands_crest.mean",
	"lowlevel.melbands_kurtosis.mean",
	"lowlevel.melbands_skewness.mean",
	"lowlevel.melbands_spread.mean",
	"lowlevel.pitch_salience.mean",
	"lowlevel.pitch_salience.var",
	"lowlevel.silence_rate_30dB.mean",
	"lowlevel.silence_rate_60dB.mean",
	"lowlevel.spectral_centroid.mean",
	"lowlevel.spectral_complexity.mean",
	"lowlevel.spectral_decrease.mean",
	"lowlevel.spectral_energy.mean",
	"lowlevel.spectral_energyband_high.mean",
	"lowlevel.spectral_energyband_low.mean",
	"lowlevel.spectral_energyband_middle_high.mean",
	"lowlevel.spectral_energyband_middle_low.mean",
	"lowlevel.spectral_entropy.mean",
	"lowlevel.spectral_flux.mean",
	"lowlevel.spectral_kurtosis.mean",
	"lowlevel.spectral_rms.mean",
	"lowlevel.spectral_skewness.mean",
	"lowlevel.spectral_spread.mean",
	"lowlevel.spectral_strongpeak.mean",
	"lowlevel.zerocrossingrate.mean",
	"rhythm.beats_loudness.mean",		// BAD
	"rhythm.bpm",
	"tonal.chords_changes_rate",		// OK
	"tonal.chords_number_rate",		// BAD
	"tonal.chords_strength.mean",		// OK
	"tonal.hpcp_entropy.mean",		// GOOD
};

} // namespace

namespace Database {


SimilaritySettings::pointer
SimilaritySettings::get(Wt::Dbo::Session& session)
{
	pointer settings = session.find<SimilaritySettings>();
	if (!settings)
	{
		settings = session.add(std::make_unique<SimilaritySettings>());
		settings.modify()->setTrackFeatureTypes(defaultTrackFeaturesNames);
	}

	return settings;
}

std::vector<Wt::Dbo::ptr<TrackFeatureType>>
SimilaritySettings::getTrackFeatureTypes() const
{
	return std::vector<Wt::Dbo::ptr<TrackFeatureType>>(_trackFeatureTypes.begin(), _trackFeatureTypes.end());
}

void
SimilaritySettings::setTrackFeatureTypes(const std::set<std::string>& featuresNames)
{
	bool needRescan = false;
	assert(session());

	// Create any missing feature type
	for (const auto& featureName : featuresNames)
	{
		auto featureType = TrackFeatureType::getByName(*session(), featureName);
		if (!featureType)
		{
			LMS_LOG(DB, INFO) << "Creating feature type " << featureName;
			featureType = TrackFeatureType::create(*session(), featureName);
			_trackFeatureTypes.insert(featureType);

			needRescan = true;
		}
	}

	// Delete no longer existing feature type
	for (auto trackFeatureType : _trackFeatureTypes)
	{
		if (std::none_of(featuresNames.begin(), featuresNames.end(),
			[trackFeatureType](const std::string& name) { return name == trackFeatureType->getName(); }))
		{
			LMS_LOG(DB, INFO) << "Deleting track feature type " << trackFeatureType->getName();
			trackFeatureType.remove();
		}
	}

	if (needRescan)
		_scanVersion += 1;
}



} // namespace Database

