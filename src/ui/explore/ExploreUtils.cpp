
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

#include "ExploreUtils.hpp"

#include "utils/Utils.hpp"

#include "database/Setting.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

std::vector<Database::ClusterType::pointer> getClusterTypesFromSetting(std::string setting)
{
	std::vector<Database::ClusterType::pointer> res;
	for (auto clusterTypeName : splitString(Database::Setting::getString(LmsApp->getDboSession(), setting), " "))
	{
		auto clusterType = Database::ClusterType::getByName(LmsApp->getDboSession(), clusterTypeName);
		if (clusterType)
			res.push_back(clusterType);
	}

	return res;
}

void setClusterTypesToSetting(std::string setting, const std::set<std::string>& clusterTypes)
{
	std::vector<std::string> vec(clusterTypes.begin(), clusterTypes.end());
	Database::Setting::setString(LmsApp->getDboSession(), setting, joinStrings(vec, " "));
}

} // namespace UserInterface

