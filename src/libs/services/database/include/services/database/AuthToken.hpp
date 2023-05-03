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

#pragma once

#include <string_view>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "services/database/AuthTokenId.hpp"
#include "services/database/Object.hpp"

namespace Database
{
	class Session;

	class User;
	class AuthToken final : public Object<AuthToken, AuthTokenId>
	{
		public:
			AuthToken() = default;

			// Utility
			static void		removeExpiredTokens(Session& session, const Wt::WDateTime& now);
			static pointer	find(Session& session, std::string_view value);

			// Accessors
			const Wt::WDateTime&	getExpiry() const { return _expiry; }
			ObjectPtr<User>			getUser() const { return _user; }
			const std::string&		getValue() const { return _value; }

			template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a,	_value,		"value");
				Wt::Dbo::field(a,	_expiry,	"expiry");
				Wt::Dbo::belongsTo(a,	_user,		"user", Wt::Dbo::OnDeleteCascade);
			}

		private:
			friend class Session;
			AuthToken(std::string_view value, const Wt::WDateTime& expiry, ObjectPtr<User> user);
			static pointer create(Session& session, std::string_view value, const Wt::WDateTime&expiry, ObjectPtr<User> user);

			std::string			_value;
			Wt::WDateTime		_expiry;
			Wt::Dbo::ptr<User>	_user;
	};
} // namespace Databas'

