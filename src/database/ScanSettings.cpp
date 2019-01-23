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

#include "ScanSettings.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "Cluster.hpp"

namespace {

std::set<std::string> defaultClusterTypeNames =
{
	"GENRE",
	"ALBUMGROUPING",
	"MOOD",
	"ALBUMMOOD",
	"COMMENT:SONGS-DB_OCCASION",
};

}

namespace Database {


ScanSettings::pointer
ScanSettings::get(Wt::Dbo::Session& session)
{
	pointer settings = session.find<ScanSettings>();
	if (!settings)
	{
		settings = session.add(std::make_unique<ScanSettings>());
		settings.modify()->setClusterTypes(defaultClusterTypeNames);
	}

	return settings;
}

std::set<boost::filesystem::path>
ScanSettings::getAudioFileExtensions() const
{
	auto extensions = splitString(_audioFileExtensions, " ");
	return std::set<boost::filesystem::path>(extensions.begin(), extensions.end());
}

std::vector<ClusterType::pointer>
ScanSettings::getClusterTypes() const
{
	return std::vector<ClusterType::pointer>(_clusterTypes.begin(), _clusterTypes.end());
}

void
ScanSettings::setMediaDirectory(boost::filesystem::path p)
{
	_mediaDirectory = stringTrimEnd(p.string(), "/\\");
}

void
ScanSettings::setClusterTypes(const std::set<std::string>& clusterTypeNames)
{
	bool needRescan = false;
	assert(session());

	// Create any missing cluster type
	for (const auto& clusterTypeName : clusterTypeNames)
	{
		auto clusterType = ClusterType::getByName(*session(), clusterTypeName);
		if (!clusterType)
		{
			LMS_LOG(DB, INFO) << "Creating cluster type " << clusterTypeName;
			clusterType = ClusterType::create(*session(), clusterTypeName);
			_clusterTypes.insert(clusterType);

			needRescan = true;
		}
	}

	// Delete no longer existing cluster types
	for (auto clusterType : _clusterTypes)
	{
		if (std::none_of(clusterTypeNames.begin(), clusterTypeNames.end(),
			[clusterType](const std::string& name) { return name == clusterType->getName(); }))
		{
			LMS_LOG(DB, INFO) << "Deleting cluster type " << clusterType->getName();
			clusterType.remove();
		}
	}

	if (needRescan)
		_scanVersion += 1;
}


} // namespace Database

