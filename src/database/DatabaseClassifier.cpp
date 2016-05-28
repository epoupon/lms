/*
 * Copyright (C) 2016 Emeric Poupon
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

//#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>

#include "logger/Logger.hpp"
#include "feature/FeatureExtractor.hpp"

#include "knnl/neural_net_headers.hpp"

#include "DatabaseClassifier.hpp"

namespace Database {

Classifier::Classifier(Wt::Dbo::SqlConnectionPool& connectionPool)
: _db(connectionPool)
{}

void
Classifier::processTrackUpdate(bool added, Track::id_type trackId, std::string mbid, boost::filesystem::path path)
{
	if (!added)
		return;

	if (mbid.empty())
	{
		// TODO compute from file
		LMS_LOG(DBUPDATER, INFO) << "File '" << path << "' has no MBID: skipping feature extraction";
		return;
	}

	boost::property_tree::ptree pt;
	if (::Feature::Extractor::getLowLevel(pt, mbid))
	{
		std::ostringstream oss;
		boost::property_tree::write_json(oss, pt);

		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
			Feature::create( _db.getSession(), track, "low_level", oss.str());
		}
	}

	pt.clear();
	if (::Feature::Extractor::getHighLevel(pt, mbid))
	{
		std::ostringstream oss;
		boost::property_tree::write_json(oss, pt);

		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
			Feature::create( _db.getSession(), track, "high_level", oss.str());
		}
	}
}

typedef std::vector<double> entry_t;
typedef std::vector<entry_t> Entries;

static bool entryAddData(entry_t& entry, const boost::property_tree::ptree& pt, std::size_t nbDimensions)
{
	if (pt.empty() && nbDimensions == 1)
	{
		double value = std::stod(pt.data());
		entry.push_back(value);

		return true;
	}

	if (pt.size() == nbDimensions)
	{
		for (auto it : pt)
		{
			double value = std::stod(it.second.data());
			entry.push_back(value);
		}

		return true;
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Bad entry";

	return false;
}

struct FeatureDesc
{
	std::string	lowLevelName;
	std::size_t	nbDimensions;
	double		coeff;
};

static std::vector<FeatureDesc> features =
{
//	{ "lowlevel.average_loudness",		1,	1.0 },
//	{ "lowlevel.barkbands.dmean",		27,	1.0 },
//	{ "lowlevel.barkbands.dmean2",		27,	1.0 },
//	{ "lowlevel.barkbands.dvar",		27,	1.0 },
//	{ "lowlevel.barkbands.dvar2",		27,	1.0 },
//	{ "lowlevel.barkbands.max",		27,	1.0 },
//	{ "lowlevel.barkbands.mean",		27,	1.0 },
//	{ "lowlevel.barkbands.median",		27,	1.0 },
//	{ "lowlevel.barkbands.min",		27,	1.0 },
//	{ "lowlevel.barkbands.var",		27,	1.0 },
//	{ "lowlevel.barkbands_crest.dmean",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.dmean2",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.dvar",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.dvar2",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.max",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.mean",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.median",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.min",	1,	1.0 },
//	{ "lowlevel.barkbands_crest.var",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.dmean",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.dmean2",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.dvar",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.dvar2",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.max",		1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.mean",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.median",	1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.min",		1,	1.0 },
//	{ "lowlevel.barkbands_flatness_db.var",		1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.dmean",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.dmean2",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.dvar",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.dvar2",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.max",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.mean",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.median",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.min",	1,	1.0 },
//	{ "lowlevel.barkbands_kurtosis.var",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.dmean",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.dmean2",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.dvar",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.dvar2",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.max",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.mean",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.median",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.min",	1,	1.0 },
//	{ "lowlevel.barkbands_skewness.var",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.dmean",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.dmean2",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.dvar",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.dvar2",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.max",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.mean",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.median",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.min",	1,	1.0 },
//	{ "lowlevel.barkbands_spread.var",	1,	1.0 },
//	{ "lowlevel.dissonance.dmean",		1,	1.0 },
//	{ "lowlevel.dissonance.dmean2",		1,	1.0 },
//	{ "lowlevel.dissonance.dvar",		1,	1.0 },
//	{ "lowlevel.dissonance.dvar2",		1,	1.0 },
//	{ "lowlevel.dissonance.max",		1,	1.0 },
//	{ "lowlevel.dissonance.mean",		1,	1.0 },
//	{ "lowlevel.dissonance.median",		1,	1.0 },
//	{ "lowlevel.dissonance.min",		1,	1.0 },
//	{ "lowlevel.dissonance.var",		1,	1.0 },
//	{ "lowlevel.dynamic_complexity",	1,	1.0 },
//	{ "lowlevel.erbbands.dmean",		40,	1.0 },
//	{ "lowlevel.erbbands.dmean2",		40,	1.0 },
//	{ "lowlevel.erbbands.dvar",		40,	1.0 },
//	{ "lowlevel.erbbands.dvar2",		40,	1.0 },
//	{ "lowlevel.erbbands.max",		40,	1.0 },
//	{ "lowlevel.erbbands.mean",		40,	1.0 },
//	{ "lowlevel.erbbands.median",		40,	1.0 },
//	{ "lowlevel.erbbands.min",		40,	1.0 },
//	{ "lowlevel.erbbands.var",		40,	1.0 },
//	{ "lowlevel.erbbands_crest.dmean",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.dmean2",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.dvar",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.dvar2",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.max",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.mean",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.median",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.min",		1,	1.0 },
//	{ "lowlevel.erbbands_crest.var",		1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.dmean",	1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.dmean2",	1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.dvar",		1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.dvar2",	1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.max",		1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.mean",		1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.median",	1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.min",		1,	1.0 },
//	{ "lowlevel.erbbands_flatness_db.var",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.dmean",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.dmean2",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.dvar",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.dvar2",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.max",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.mean",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.median",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.min",		1,	1.0 },
//	{ "lowlevel.erbbands_kurtosis.var",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.dmean",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.dmean2",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.dvar",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.dvar2",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.max",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.mean",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.median",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.min",		1,	1.0 },
//	{ "lowlevel.erbbands_skewness.var",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.dmean",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.dmean2",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.dvar",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.dvar2",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.max",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.mean",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.median",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.min",		1,	1.0 },
//	{ "lowlevel.erbbands_spread.var",		1,	1.0 },
//	{ "lowlevel.gfcc.mean",				13,	1.0 },
//	{ "lowlevel.hfc.dmean",				1,	1.0 },
//	{ "lowlevel.hfc.dmean2",			1,	1.0 },
//	{ "lowlevel.hfc.dvar",				1,	1.0 },
//	{ "lowlevel.hfc.dvar2",				1,	1.0 },
//	{ "lowlevel.hfc.max",				1,	1.0 },
//	{ "lowlevel.hfc.mean",				1,	1.0 },
//	{ "lowlevel.hfc.median",			1,	1.0 },
//	{ "lowlevel.hfc.min",				1,	1.0 },
//	{ "lowlevel.hfc.var",				1,	1.0 },
//	{ "lowlevel.melbands.dmean",			40,	1.0 },
//	{ "lowlevel.melbands.dmean2",			40,	1.0 },
//	{ "lowlevel.melbands.dvar",			40,	1.0 },
//	{ "lowlevel.melbands.dvar2",			40,	1.0 },
//	{ "lowlevel.melbands.max",			40,	1.0 },
//	{ "lowlevel.melbands.mean",			40,	1.0 },
//	{ "lowlevel.melbands.median",			40,	1.0 },
//	{ "lowlevel.melbands.min",			40,	1.0 },
//	{ "lowlevel.melbands.var",			40,	1.0 },
//	{ "lowlevel.melbands_crest.dmean",		1,	1.0 },
//	{ "lowlevel.melbands_crest.dmean2",		1,	1.0 },
//	{ "lowlevel.melbands_crest.dvar",		1,	1.0 },
//	{ "lowlevel.melbands_crest.dvar2",		1,	1.0 },
//	{ "lowlevel.melbands_crest.max",		1,	1.0 },
//	{ "lowlevel.melbands_crest.mean",		1,	1.0 },
//	{ "lowlevel.melbands_crest.median",		1,	1.0 },
//	{ "lowlevel.melbands_crest.min",		1,	1.0 },
//	{ "lowlevel.melbands_crest.var",		1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.dmean",	1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.dmean2",	1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.dvar",		1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.dvar2",	1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.max",		1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.mean",		1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.median",	1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.min",		1,	1.0 },
//	{ "lowlevel.melbands_flatness_db.var",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.dmean",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.dmean2",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.dvar",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.dvar2",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.max",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.mean",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.median",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.min",		1,	1.0 },
//	{ "lowlevel.melbands_kurtosis.var",		1,	1.0 },
//	{ "lowlevel.melbands_spread.dmean",		1,	1.0 },
//	{ "lowlevel.melbands_spread.dmean2",		1,	1.0 },
//	{ "lowlevel.melbands_spread.dvar",		1,	1.0 },
//	{ "lowlevel.melbands_spread.dvar2",		1,	1.0 },
//	{ "lowlevel.melbands_spread.max",		1,	1.0 },
//	{ "lowlevel.melbands_spread.mean",		1,	1.0 },
//	{ "lowlevel.melbands_spread.median",		1,	1.0 },
//	{ "lowlevel.melbands_spread.min",		1,	1.0 },
//	{ "lowlevel.melbands_spread.var",		1,	1.0 },
//	{ "lowlevel.mfcc.mean",				13,	1.0 },
//	{ "lowlevel.pitch_salience.dmean",		1,	1.0 },
//	{ "lowlevel.pitch_salience.dmean2",		1,	1.0 },
//	{ "lowlevel.pitch_salience.dvar",		1,	1.0 },
//	{ "lowlevel.pitch_salience.dvar2",		1,	1.0 },
//	{ "lowlevel.pitch_salience.max",		1,	1.0 },
//	{ "lowlevel.pitch_salience.mean",		1,	1.0 },
//	{ "lowlevel.pitch_salience.median",		1,	1.0 },
//	{ "lowlevel.pitch_salience.min",		1,	1.0 },
//	{ "lowlevel.pitch_salience.var",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.dmean",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.dmean2",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.dvar",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.dvar2",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.max",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.mean",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.median",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.min",		1,	1.0 },
//	{ "lowlevel.silence_rate_20dB.var",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.dmean",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.dmean2",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.dvar",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.dvar2",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.max",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.mean",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.median",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.min",		1,	1.0 },
//	{ "lowlevel.silence_rate_30dB.var",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.dmean",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.dmean2",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.dvar",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.dvar2",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.max",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.mean",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.median",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.min",		1,	1.0 },
//	{ "lowlevel.silence_rate_60dB.var",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.max",		1,	1.0 },
	{ "lowlevel.spectral_centroid.mean",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.median",		1,	1.0 },
//	{ "lowlevel.spectral_centroid.min",		1,	1.0 },
	{ "lowlevel.spectral_centroid.var",		1,	1.0 },
//	{ "lowlevel.spectral_complexity.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_complexity.dmean2",	1,	1.0 },
//	{ "lowlevel.spectral_complexity.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_complexity.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_complexity.max",		1,	1.0 },
	{ "lowlevel.spectral_complexity.mean",		1,	1.0 },
//	{ "lowlevel.spectral_complexity.median",	1,	1.0 },
//	{ "lowlevel.spectral_complexity.min",		1,	1.0 },
	{ "lowlevel.spectral_complexity.var",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.max",		1,	1.0 },
	{ "lowlevel.spectral_decrease.mean",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.median",		1,	1.0 },
//	{ "lowlevel.spectral_decrease.min",		1,	1.0 },
	{ "lowlevel.spectral_decrease.var",		1,	1.0 },
//	{ "lowlevel.spectral_energy.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_energy.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_energy.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_energy.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_energy.max",		1,	1.0 },
	{ "lowlevel.spectral_energy.mean",		1,	1.0 },
//	{ "lowlevel.spectral_energy.median",		1,	1.0 },
//	{ "lowlevel.spectral_energy.min",		1,	1.0 },
	{ "lowlevel.spectral_energy.var",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.max",		1,	1.0 },
	{ "lowlevel.spectral_energyband_low.mean",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.median",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_low.min",		1,	1.0 },
	{ "lowlevel.spectral_energyband_low.var",		1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.dmean",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.dmean2",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.dvar",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.dvar2",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.max",	1,	1.0 },
	{ "lowlevel.spectral_energyband_middle_high.mean",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.median",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_high.min",	1,	1.0 },
	{ "lowlevel.spectral_energyband_middle_high.var",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.dmean",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.dmean2",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.dvar",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.dvar2",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.max",	1,	1.0 },
	{ "lowlevel.spectral_energyband_middle_low.mean",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.median",	1,	1.0 },
//	{ "lowlevel.spectral_energyband_middle_low.min",	1,	1.0 },
	{ "lowlevel.spectral_energyband_middle_low.var",	1,	1.0 },
//	{ "lowlevel.spectral_entropy.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_entropy.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_entropy.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_entropy.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_entropy.max",		1,	1.0 },
	{ "lowlevel.spectral_entropy.mean",		1,	1.0 },
//	{ "lowlevel.spectral_entropy.median",		1,	1.0 },
//	{ "lowlevel.spectral_entropy.min",		1,	1.0 },
	{ "lowlevel.spectral_entropy.var",		1,	1.0 },
//	{ "lowlevel.spectral_flux.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_flux.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_flux.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_flux.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_flux.max",			1,	1.0 },
//	{ "lowlevel.spectral_flux.mean",		1,	1.0 },
//	{ "lowlevel.spectral_flux.median",		1,	1.0 },
//	{ "lowlevel.spectral_flux.min",			1,	1.0 },
//	{ "lowlevel.spectral_flux.var",			1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.max",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.mean",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.median",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.min",		1,	1.0 },
//	{ "lowlevel.spectral_kurtosis.var",		1,	1.0 },
//	{ "lowlevel.spectral_rms.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_rms.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_rms.dvar",			1,	1.0 },
//	{ "lowlevel.spectral_rms.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_rms.max",			1,	1.0 },
	{ "lowlevel.spectral_rms.mean",			1,	1.0 },
//	{ "lowlevel.spectral_rms.median",		1,	1.0 },
//	{ "lowlevel.spectral_rms.min",			1,	1.0 },
	{ "lowlevel.spectral_rms.var",			1,	1.0 },
//	{ "lowlevel.spectral_rolloff.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.max",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.mean",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.median",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.min",		1,	1.0 },
//	{ "lowlevel.spectral_rolloff.var",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.max",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.mean",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.median",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.min",		1,	1.0 },
//	{ "lowlevel.spectral_skewness.var",		1,	1.0 },
//	{ "lowlevel.spectral_spread.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_spread.dmean2",		1,	1.0 },
//	{ "lowlevel.spectral_spread.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_spread.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_spread.max",		1,	1.0 },
	{ "lowlevel.spectral_spread.mean",		1,	1.0 },
//	{ "lowlevel.spectral_spread.median",		1,	1.0 },
//	{ "lowlevel.spectral_spread.min",		1,	1.0 },
	{ "lowlevel.spectral_spread.var",		1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.dmean",		1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.dmean2",	1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.dvar",		1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.dvar2",		1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.max",		1,	1.0 },
	{ "lowlevel.spectral_strongpeak.mean",		1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.median",	1,	1.0 },
//	{ "lowlevel.spectral_strongpeak.min",		1,	1.0 },
	{ "lowlevel.spectral_strongpeak.var",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.dmean",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.dmean2",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.dvar",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.dvar2",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.max",		1,	1.0 },
	{ "lowlevel.zerocrossingrate.mean",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.median",		1,	1.0 },
//	{ "lowlevel.zerocrossingrate.min",		1,	1.0 },
	{ "lowlevel.zerocrossingrate.var",		1,	1.0 },
//	{ "rhythm.beats_count",				1,	1.0 },
//	{ "rhythm.beats_loudness.dmean",		1,	1.0 },
//	{ "rhythm.beats_loudness.dmean2",		1,	1.0 },
//	{ "rhythm.beats_loudness.dvar",			1,	1.0 },
//	{ "rhythm.beats_loudness.dvar2",		1,	1.0 },
//	{ "rhythm.beats_loudness.max",			1,	1.0 },
	{ "rhythm.beats_loudness.mean",			1,	1.0 },
//	{ "rhythm.beats_loudness.median",		1,	1.0 },
//	{ "rhythm.beats_loudness.min",			1,	1.0 },
	{ "rhythm.beats_loudness.var",			1,	1.0 },
//	{ "rhythm.bpm",					1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.dmean",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.dmean2",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.dvar",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.dvar2",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.max",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.mean",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.median",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.min",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_bpm.var",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.dmean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.dmean2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.dvar",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.dvar2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.max",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.mean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.median",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.min",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_spread.var",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.dmean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.dmean2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.dvar",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.dvar2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.max",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.mean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.median",	1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.min",		1,	1.0 },
//	{ "rhythm.bpm_histogram_first_peak_weight.var",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.dmean",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.dmean2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.dvar",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.dvar2",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.max",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.mean",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.median",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.min",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_bpm.var",		1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.dmean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.dmean2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.dvar",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.dvar2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.max",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.mean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.median",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.min",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_spread.var",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.dmean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.dmean2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.dvar",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.dvar2",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.max",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.mean",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.median",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.min",	1,	1.0 },
//	{ "rhythm.bpm_histogram_second_peak_weight.var",	1,	1.0 },
//	{ "rhythm.danceability",				1,	1.0},
//	{ "rhythm.onset_rate",				1,	1.0},
//
//	{ "tonal.chords_changes_rate",		1,	1.0 },
//	{ "tonal.chords_histogram",		24,	1.0 },
//	{ "tonal.chords_number_rate",		1,	1.0 },
//	{ "tonal.hpcp_entropy.dmean",		1,	1.0 },
//	{ "tonal.hpcp_entropy.dmean2",		1,	1.0 },
//	{ "tonal.hpcp_entropy.dvar",		1,	1.0 },
//	{ "tonal.hpcp_entropy.dvar2",		1,	1.0 },
//	{ "tonal.hpcp_entropy.max",		1,	1.0 },
//	{ "tonal.hpcp_entropy.mean",		1,	1.0 },
//	{ "tonal.hpcp_entropy.median",		1,	1.0 },
//	{ "tonal.hpcp_entropy.min",		1,	1.0 },
//	{ "tonal.hpcp_entropy.var",		1,	1.0 },
//	{ "tonal.tuning_frequency",		1,	1.0 },
};

static bool entryConstructFromJSON(entry_t& entry, const std::string& jsonData)
{
	std::istringstream iss(jsonData);
	boost::property_tree::ptree pt;
	boost::property_tree::json_parser::read_json(iss, pt);

	for (auto& elem : features)
	{
		auto child = pt.get_child_optional(elem.lowLevelName);

		if (!child)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Cannot get '" << elem.lowLevelName << "'";
			return false;
		}

		if (!entryAddData(entry, *child,  elem.nbDimensions))
			return false;
	}

	return true;
}

static entry_t computeWeightCoeffs(const Entries& entries)
{
	std::vector<FeatureDesc> expandedFeatures;
	std::vector<double> userCoeffs;

	for (auto& feature : features)
	{
		for (std::size_t i = 0; i < feature.nbDimensions; ++i)
		{
			expandedFeatures.push_back( { feature.lowLevelName + std::to_string(i), 1, feature.coeff / (double)feature.nbDimensions} );
		}
	}

	entry_t coeffs;

	::neural_net::Ranges<Entries> ranges (*entries.begin());
	ranges( entries );

	entry_t::const_iterator pos_max = ranges.get_max().begin();
	entry_t::const_iterator pos_min = ranges.get_min().begin();
	for (std::size_t i = 0; pos_max != ranges.get_max().end(); ++pos_max, ++pos_min, ++i)
	{
		coeffs.push_back( expandedFeatures.at(i).coeff *
				::operators::inverse (
					( *pos_max - *pos_min ) * ( *pos_max - *pos_min )
					) ); // weight for i-th axis
		std::cout << "Feature = " << expandedFeatures.at(i).lowLevelName << std::endl;
		std::cout << "min = " << *pos_min << ", max = " << *pos_max << std::endl;
		std::cout << "Coeff = " << coeffs.back() << std::endl;
	}
	return coeffs;
}
	template <class T, size_t ROW, size_t COL>
		using Matrix = std::array<std::array<T, COL>, ROW>;

void
Classifier::processDatabaseUpdate(Updater::Stats stats)
{
	LMS_LOG(DBUPDATER, DEBUG) << "Database complete Called!";

//	typedef ::neural_net::Cauchy_function < entry_t::value_type, entry_t::value_type, ::boost::int32_t> CauchyFunction;

	typedef ::neural_net::Gauss_function < entry_t::value_type, entry_t::value_type, ::boost::int32_t > GaussFunction;

//	typedef ::distance::Euclidean_distance_function < entry_t > EuclideanDistFunction;
	typedef ::distance::Weighted_euclidean_distance_function < entry_t, entry_t > WeightedEuclideanDistFunction;

	typedef ::neural_net::Basic_neuron < GaussFunction, WeightedEuclideanDistFunction > KohonenNeuron;
	typedef ::neural_net::Rectangular_container < KohonenNeuron > KohonenNetwork;

//	typedef ::neural_net::Hexagonal_topology < ::boost::int32_t > HexagonalTopology;
	typedef ::neural_net::Max_topology < ::boost::int32_t > MaxTopology;

	typedef GaussFunction GaussFunctionSpace;
	typedef ::neural_net::Gauss_function < ::boost::int32_t, entry_t::value_type, ::boost::int32_t > GaussFunctionNet;
//	typedef ::neural_net::Constant_function < entry_t::value_type, entry_t::value_type > ConstFuncSpace;

	typedef ::neural_net::Classic_training_weight
		<
		entry_t,
		::boost::int32_t,
		GaussFunctionNet,
		GaussFunctionSpace,
		MaxTopology,
		WeightedEuclideanDistFunction,
		::boost::int32_t
			> ClassicWeight;

	typedef ::neural_net::Wtm_classical_training_functional
		<
		entry_t,
		double,
		::boost::int32_t,
		::boost::int32_t,
		ClassicWeight
			> WtmTrainingFunc;

	typedef ::neural_net::Wtm_training_algorithm
		<
		KohonenNetwork,
		entry_t,
		Entries::iterator,
		WtmTrainingFunc,
		::boost::int32_t
			> WtmTrainingAlg;

	Entries entries;

	LMS_LOG(DBUPDATER, DEBUG) << "Getting track ids";

	std::vector<Track::id_type> trackIdsAll = Database::Track::getAllIds(_db.getSession());

	::std::random_shuffle ( trackIdsAll.begin(), trackIdsAll.end() );

	LMS_LOG(DBUPDATER, DEBUG) << "Getting JSON data";
	std::vector<Track::id_type> trackIds;
	for (Track::id_type trackId : trackIdsAll)
	{
		std::string jsonData;

		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			std::vector<Feature::pointer> features = Feature::getByTrack(_db.getSession(), trackId, "low_level");

			if (features.empty())
			{
				LMS_LOG(DBUPDATER, DEBUG) << "No JSON data for track " << trackId;;
				continue;
			}

			jsonData = features.front()->getValue();
		}

		entry_t entry;
		if (entryConstructFromJSON(entry, jsonData))
		{
			trackIds.push_back(trackId);
			entries.push_back(entry);
		}
		else
		{
			LMS_LOG(DBUPDATER, ERROR) << "Skipping track " << trackId;
			continue;
		}

//		if (entries.size() > 100)
//			break;
	}
	std::cout << "Feature vector dimension = " << entries.front().size() << std::endl;

	if (entries.empty())
		return;

	// TODO compute rows / columns from the DBB
	const std::size_t nbRows = 32;
	const std::size_t nbColumns = 32;

	//	CauchyFunction cauchyFunc(2.0, 1);
	GaussFunction gaussFunc(2.0, 1 );

	LMS_LOG(DBUPDATER, DEBUG) << "Computing coeffs";
	entry_t coeffs = computeWeightCoeffs(entries);
	LMS_LOG(DBUPDATER, DEBUG) << "Initializing distfunc";
	WeightedEuclideanDistFunction weightedEuclideanDistFunc (&coeffs);

	// prepare randomization policy
	::neural_net::Internal_randomize internalRandomize;

	KohonenNetwork network;

	// generate networks initialized by data
	LMS_LOG(DBUPDATER, DEBUG) << "Generating network...";
	::neural_net::generate_kohonen_network(nbRows, nbColumns, gaussFunc, weightedEuclideanDistFunc, entries, network, internalRandomize);

	::std::cout << "Network weights:" << ::std::endl;
	::neural_net::print_network_weights ( ::std::cout, network );
	::std::cout << ::std::endl;

	GaussFunctionNet gaussFuncNetwork(10, 1);
	GaussFunctionSpace gaussFuncSpace(10, 1);
	MaxTopology maxTopology;
//	EuclideanDistFunction euclideanDistFunc;

	ClassicWeight classicWeight(gaussFuncNetwork, gaussFuncSpace, maxTopology, weightedEuclideanDistFunc);
	WtmTrainingFunc trainingFunc(classicWeight, 0.3);

	WtmTrainingAlg wtmTrainAlg( trainingFunc );

	LMS_LOG(DBUPDATER, DEBUG) << "Training...";
	// tricky training
	std::size_t nbPass = 20;
	for (std::size_t i = 0; i < nbPass; ++i )
	{
		LMS_LOG(DBUPDATER, DEBUG) << "Training pass " << i << " / " << nbPass;
		// train network using data
		auto entries_copy = entries;
		::std::random_shuffle ( entries_copy.begin(), entries_copy.end() );

		wtmTrainAlg(entries_copy.begin(), entries_copy.end(), &network);

		// decrease sigma parameter in network will make training proces more sharpen with each epoch,
		// but it have to be done slowly :-)
	wtmTrainAlg.training_functional.generalized_training_weight.network_function.sigma *= 2.0/3.0;
//		wtmTrainAlg.training_functional.generalized_training_weight.network_function.sigma *= 9.0/10.0;

		// shuffle data
//		::std::random_shuffle ( entries.begin(), entries.end() );
	}
	LMS_LOG(DBUPDATER, DEBUG) << "Training DONE...";

	::std::cout << "Network weights:" << ::std::endl;
	::neural_net::print_network_weights ( ::std::cout, network );
	::std::cout << ::std::endl;

	struct ClusterEntry
	{
		Track::id_type trackId;
		entry_t	entry;
		double distance;
	};

	Matrix<std::vector<ClusterEntry>, nbRows, nbColumns> trackClusters;

	for (std::size_t id = 0; id < entries.size(); ++id)
	{
		std::cout << "Entry " << id << " : " << std::endl;
		// Display input vector
		for (auto& value : entries[id])
		{
			std::cout << value << " ";
		}
		std::cout << std::endl;

		auto entry = entries[id];
		std::pair<int, int> coordinates = {0,0};
		double maxValue = 0.0;

		for (std::size_t i = 0; i < network.objects.size(); ++i)
		{
			for (std::size_t j = 0; j < network.objects[0].size(); ++j)
			{
				double value = network.objects[i][j]( entry);
				if (value > maxValue)
				{
					coordinates = {i, j};
					maxValue = value;
				}
			}
		}

/*		::std::cout << "Network for entry " << id << std::endl;
		::neural_net::print_network ( ::std::cout, network, entry );
		::std::cout << ::std::endl;

		std::cout << "Max is in {" << coordinates.first << ", " << coordinates.second << "}" << std::endl;
		*/
		trackClusters[coordinates.first][coordinates.second].push_back({trackIds[id], entry, maxValue});
	}

	// Create clusters
	LMS_LOG(DBUPDATER, DEBUG) << "Erasing old clusters";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());
		Cluster::removeByType(_db.getSession(), "similarity");
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Creating new cluster...";
	for (std::size_t i = 0; i < nbRows; i++)
	{
		for (std::size_t j = 0; j < nbColumns; j++)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Creating cluster " << i << " " << j;

			Wt::Dbo::Transaction transaction(_db.getSession());

			Cluster::pointer cluster = Cluster::create(_db.getSession(), "similarity", "cluster_" + std::to_string(i) + "_" + std::to_string(j));

			for (auto track : trackClusters[i][j])
			{
				Track::pointer t = Database::Track::getById(_db.getSession(), track.trackId);
				cluster.modify()->addTrack(t);
			}
		}
	}


	for (std::size_t i = 0; i < nbRows; i++)
	{
		for (std::size_t j = 0; j < nbColumns; j++)
		{


			std::cout << "Cluster [" << i << "," << j << "] - ";

			// Display the neuron for this cluster
			for (auto& value : network.objects[i][j].weights)
				std::cout << value << " ";
			std::cout << std::endl;

			for (auto track : trackClusters[i][j])
			{
				Wt::Dbo::Transaction transaction(_db.getSession());

				std::cout << "- " << track.distance;

				Track::pointer t = Database::Track::getById(_db.getSession(), track.trackId);
				std::cout << " - " << track.trackId << " - " << t->getArtist()->getName() << " - " << t->getName() << " - (" ;

				// Display input vector
				for (auto& value : track.entry)
					std::cout << value << " ";
				std::cout << ")" << std::endl;
			}
		}
	}


}

} // namespace Database

