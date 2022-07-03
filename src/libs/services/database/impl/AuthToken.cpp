/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "services/database/AuthToken.hpp"

#include <Wt/Dbo/WtSqlTraits.h>
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "StringViewTraits.hpp"
#include "IdTypeTraits.hpp"

namespace Database
{

	AuthToken::AuthToken(std::string_view value, const Wt::WDateTime& expiry, ObjectPtr<User> user)
	: _value {value}
	, _expiry {expiry}
	, _user {getDboPtr(user)}
	{
	}

	AuthToken::pointer
	AuthToken::create(Session& session, std::string_view value, const Wt::WDateTime& expiry, ObjectPtr<User> user)
	{
		return session.getDboSession().add(std::unique_ptr<AuthToken> {new AuthToken {value, expiry, user}});
	}

	void
	AuthToken::removeExpiredTokens(Session& session, const Wt::WDateTime& now)
	{
		session.checkUniqueLocked();

		session.getDboSession().execute("DELETE FROM auth_token WHERE expiry < ?").bind(now);
	}

	AuthToken::pointer
	AuthToken::find(Session& session, std::string_view value)
	{
		session.checkSharedLocked();

		return session.getDboSession().find<AuthToken>()
			.where("value = ?").bind(value)
			.resultValue();
	}
}
