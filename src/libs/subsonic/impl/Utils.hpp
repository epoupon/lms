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
#pragma once

#include <string>
#include <Wt/Dbo/ptr.h>

#include "SubsonicResponse.hpp"

namespace Database
{
	class Artist;
	class Session;
	class Track;
	class User;
}

namespace API::Subsonic
{
	static inline const std::string reportedStarredDate {"2000-01-01T00:00:00"};
	static inline const std::string genreClusterName {"GENRE"};


	std::string makeNameFilesystemCompatible(const std::string& name);

	std::string getArtistNames(const std::vector<Wt::Dbo::ptr<Database::Artist>>& artists);

	Response::Node trackToResponseNode(const Wt::Dbo::ptr<Database::Track>& track, Database::Session& dbSession, const Wt::Dbo::ptr<Database::User>& user);

}

