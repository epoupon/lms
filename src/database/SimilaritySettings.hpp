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

class SimilaritySettings;

class SimilaritySettingsFeature :  public Wt::Dbo::Dbo<SimilaritySettingsFeature>
{
	public:
		using pointer = Wt::Dbo::ptr<SimilaritySettingsFeature>;

		SimilaritySettingsFeature() = default;
		SimilaritySettingsFeature(Wt::Dbo::ptr<SimilaritySettings> settings, const std::string& name, std::size_t nbDimensions, double weight);

		static pointer create(Wt::Dbo::Session& session, Wt::Dbo::ptr<SimilaritySettings> settings, const std::string& name, std::size_t nbDimensions, double weight = 1);

		const std::string&	getName() const { return _name; } ;
		std::size_t 		getNbDimensions() const { return static_cast<std::size_t>(_nbDimensions); }
		double			getWeight() const { return _weight; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _name,		"name");
			Wt::Dbo::field(a, _nbDimensions,	"dimension_count");
			Wt::Dbo::field(a, _weight,		"weight");

			Wt::Dbo::belongsTo(a, _settings, "similarity_settings", Wt::Dbo::OnDeleteCascade);
		}

	private:
		std::string	_name;
		int 		_nbDimensions;
		double		_weight;

		Wt::Dbo::ptr<SimilaritySettings> _settings;
};

class SimilaritySettings : public Wt::Dbo::Dbo<SimilaritySettings>
{
	public:

		enum class PreferredMethod
		{
			Auto,
			Features,
			Clusters,
		};

		using pointer = Wt::Dbo::ptr<SimilaritySettings>;

		// Utils
		static pointer get(Wt::Dbo::Session& session);

		// Accessors Read
		std::size_t		getVersion() const { return _settingsVersion; }
		PreferredMethod		getPreferredMethod() const { return _preferredMethod; }
		std::vector<Wt::Dbo::ptr<SimilaritySettingsFeature>> getFeatures() const;


		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _settingsVersion,		"settings_version");
			Wt::Dbo::field(a, _preferredMethod,		"preferred_method");

			Wt::Dbo::hasMany(a, _features, Wt::Dbo::ManyToOne, "similarity_settings");
		}

	private:

		int			_settingsVersion = 0;
		PreferredMethod		_preferredMethod = PreferredMethod::Auto;

		Wt::Dbo::collection<Wt::Dbo::ptr<SimilaritySettingsFeature>> _features;
};


} // namespace Database

