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

#include <Wt/Dbo/Dbo.h>

namespace Database {

class TrackFeature;
class TrackFeatureType;

class SimilaritySettings : public Wt::Dbo::Dbo<SimilaritySettings>
{
	public:
		using pointer = Wt::Dbo::ptr<SimilaritySettings>;

		static pointer get(Wt::Dbo::Session& session);

		std::size_t getVersion() const { return _scanVersion; }

		std::vector<Wt::Dbo::ptr<TrackFeatureType>> getTrackFeatureTypes() const;
		void setTrackFeatureTypes(const std::set<std::string>& featureTypeNames);

		void setNetworkData(std::string data) { _refFeaturesData = data; }
		const std::string& getNetworkData() const { return _refFeaturesData; }

		void setNormalizationData(std::string data) { _normalizationData = data; }
		const std::string& getNormalizationData() const { return _normalizationData; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _scanVersion,		"settings_version");
			Wt::Dbo::field(a, _normalizationData,	"normalization_data");
			Wt::Dbo::field(a, _refFeaturesData,	"ref_features_data");
			Wt::Dbo::hasMany(a, _trackFeatureTypes, Wt::Dbo::ManyToOne, "similarity_settings");
		}

	private:

		int		_scanVersion = 0;
		std::string	_normalizationData;
		std::string	_refFeaturesData;
		Wt::Dbo::collection<Wt::Dbo::ptr<TrackFeatureType>>	_trackFeatureTypes;
};


} // namespace Database

