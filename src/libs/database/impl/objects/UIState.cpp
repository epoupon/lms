/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "database/objects/UIState.hpp"

#include <Wt/Dbo/Impl.h>

#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "UIStateDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::UIState)

namespace lms::db
{
    UIState::UIState(std::string_view item)
        : _item{ item }
    {
    }

    dbo::UIState::UIState(std::string_view item, const ObjectPtr<db::User>& user)
        : db::UIState{ item }
        , _user{ getDboPtr(user) }
    {
    }

    UIState::pointer UIState::create(ISession& session, std::string_view item, const ObjectPtr<User>& user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::UIState>{ new dbo::UIState{ item, user } });
    }

    std::size_t UIState::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM ui_state"));
    }

    UIState::pointer UIState::find(ISession& session, UIStateId settingId)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::UIState>>("SELECT ui_s from ui_state ui_s").where("ui_s.id = ?").bind(settingId) };
        return utils::fetchQuerySingleResult(query);
    }

    UIState::pointer UIState::find(ISession& session, std::string_view item, UserId userId)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::UIState>>("SELECT ui_s from ui_state ui_s").where("ui_s.item = ?").bind(item).where("ui_s.user_id = ?").bind(userId) };
        return utils::fetchQuerySingleResult(query);
    }
} // namespace lms::db