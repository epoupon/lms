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

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WTime.h>

namespace Database {

class ClusterType;
class ScanSettings : public Wt::Dbo::Dbo<ScanSettings>
{
	public:
		using pointer = Wt::Dbo::ptr<ScanSettings>;

		enum class UpdatePeriod {
			Never = 0,
			Daily,
			Weekly,
			Monthly
		};

		ScanSettings() {}

		static pointer get(Wt::Dbo::Session& session);

		// Getters
		std::size_t getScanVersion() const { return _scanVersion; }
		boost::filesystem::path getMediaDirectory() const { return _mediaDirectory; }
		Wt::WTime getUpdateStartTime() const { return _startTime; }
		UpdatePeriod getUpdatePeriod() const { return _updatePeriod; }
		std::vector<Wt::Dbo::ptr<ClusterType>> getClusterTypes() const;
		std::set<boost::filesystem::path> getAudioFileExtensions() const;

		// Setters
		void setMediaDirectory(boost::filesystem::path p);
		void setUpdateStartTime(Wt::WTime t) { _startTime = t; }
		void setUpdatePeriod(UpdatePeriod p) { _updatePeriod = p; }
		void setClusterTypes(const std::set<std::string>& clusterTypeNames);
		void setAudioFileExtensions(std::set<boost::filesystem::path> fileExtensions);

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _scanVersion,		"scan_version");
			Wt::Dbo::field(a, _mediaDirectory,	"media_directory");
			Wt::Dbo::field(a, _startTime,		"start_time");
			Wt::Dbo::field(a, _updatePeriod,	"update_period");
			Wt::Dbo::field(a, _audioFileExtensions,	"audio_file_extensions");
			Wt::Dbo::hasMany(a, _clusterTypes, Wt::Dbo::ManyToOne, "scan_settings");
		}

	private:

		int		_scanVersion = 0;
		std::string	_mediaDirectory = "";
		Wt::WTime	_startTime = Wt::WTime(0,0,0);
		UpdatePeriod	_updatePeriod = UpdatePeriod::Never;
		std::string	_audioFileExtensions = ".mp3 .ogg .oga .aac .m4a .flac .wav .wma .aif .aiff .ape .mpc .shn";
		Wt::Dbo::collection<Wt::Dbo::ptr<ClusterType>>	_clusterTypes;
};


} // namespace Database

