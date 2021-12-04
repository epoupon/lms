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

#include "Utils.hpp"

#include <string_view>

#include "services/database/Session.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/User.hpp"

static constexpr std::string_view historyTracklistName {"__scrobbler_listenbrainz_history__"};

namespace Scrobbling::ListenBrainz::Utils
{
	std::optional<UUID>
	getListenBrainzToken(Database::Session& session, Database::UserId userId)
	{
		auto transaction {session.createSharedTransaction()};

		const Database::User::pointer user {Database::User::find(session, userId)};
		if (!user)
			return std::nullopt;

		if (user->getScrobbler() != Database::Scrobbler::ListenBrainz)
			return std::nullopt;

		return user->getListenBrainzToken();
	}
}
