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

#include "AuthServiceBase.hpp"

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"

namespace lms::auth
{
    using namespace db;

    AuthServiceBase::AuthServiceBase(db::IDb& db)
        : _db{ db }
    {
    }

    UserId AuthServiceBase::getOrCreateUser(std::string_view loginName)
    {
        Session& session{ getDbSession() };

        User::pointer user;

        {
            auto transaction{ session.createReadTransaction() };
            user = User::find(session, loginName);
        }

        if (!user)
        {
            auto transaction{ session.createWriteTransaction() };

            user = User::find(session, loginName);
            if (!user)
            {
                const UserType type{ User::getCount(session) == 0 ? UserType::ADMIN : UserType::REGULAR };

                LMS_LOG(AUTH, DEBUG, "Creating user '" << loginName << "', admin = " << (type == UserType::ADMIN));

                user = session.create<User>(loginName);
                user.modify()->setType(type);
            }
        }

        return user->getId();
    }

    void AuthServiceBase::onUserAuthenticated(UserId userId)
    {
        Session& session{ getDbSession() };

        // Update last login only if relevant (avoid hammering write accesses to the database)
        {
            auto transaction{ session.createReadTransaction() };

            const User::pointer user{ User::find(session, userId) };
            if (!user)
                return;

            if (std::abs(Wt::WDateTime::currentDateTime().secsTo(user->getLastLogin())) < 60)
                return;
        }

        {
            auto transaction{ session.createWriteTransaction() };

            if (User::pointer user{ User::find(session, userId) })
                user.modify()->setLastLogin(Wt::WDateTime::currentDateTime());
        }
    }

    Session& AuthServiceBase::getDbSession()
    {
        return _db.getTLSSession();
    }
} // namespace lms::auth
