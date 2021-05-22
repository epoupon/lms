/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <Wt/Dbo/ptr.h>
#include "utils/UUID.hpp"
#include "database/Types.hpp"

namespace Database
{
	class Session;
	class TrackList;
	class User;
}

namespace Scrobbling::ListenBrainz::Utils
{
	std::optional<UUID>					getListenBrainzToken(Database::Session& session, Database::IdType userId);
	Wt::Dbo::ptr<Database::TrackList>	getOrCreateListensTrackList(Database::Session& session, Wt::Dbo::ptr<Database::User> user);
	Wt::Dbo::ptr<Database::TrackList>	getListensTrackList(Database::Session& session, Wt::Dbo::ptr<Database::User> user);
}
