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

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"

namespace Scrobbling
{
	using namespace Database;

	template <typename ObjType, typename ObjIdType, typename StarredObjType>
	void
	ScrobblingService::star(UserId userId, ObjIdType objId)
	{
		auto scrobbler {getUserScrobbler(userId)};
		if (!scrobbler)
			return;

		{
			Session& session {_db.getTLSSession()};
			auto transaction {session.createUniqueTransaction()};

			typename StarredObjType::pointer starredObj {StarredObjType::find(session, objId, userId, *scrobbler)};
			if (!starredObj)
			{
				const typename ObjType::pointer obj {ObjType::find(session, objId)};
				if (!obj)
					return;

				const User::pointer user {User::find(session, userId)};
				if (!user)
					return;

				starredObj = StarredObjType::create(session, obj, user, *scrobbler);
			}
			starredObj.modify()->setDateTime(Wt::WDateTime::currentDateTime());
		}
		_scrobblers[*scrobbler]->onStarred(userId, objId);
	}

	template <typename ObjType, typename ObjIdType, typename StarredObjType>
	void
	ScrobblingService::unstar(UserId userId, ObjIdType objId)
	{
		auto scrobbler {getUserScrobbler(userId)};
		if (!scrobbler)
			return;

		{
			Session& session {_db.getTLSSession()};
			auto transaction {session.createUniqueTransaction()};

			if (typename StarredObjType::pointer starredObj {StarredObjType::find(session, objId, userId, *scrobbler)})
				starredObj.remove();
		}
		_scrobblers[*scrobbler]->onUnstarred(userId, objId);
	}

	template <typename ObjType, typename ObjIdType, typename StarredObjType>
	bool
	ScrobblingService::isStarred(UserId userId, ObjIdType objId)
	{
		auto scrobbler {getUserScrobbler(userId)};
		if (!scrobbler)
			return false;

		Session& session {_db.getTLSSession()};
		auto transaction {session.createSharedTransaction()};

		return StarredObjType::find(session, objId, userId, *scrobbler);
	}

} // ns Scrobbling

