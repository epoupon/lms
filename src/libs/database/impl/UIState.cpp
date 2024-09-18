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

#include "database/UIState.hpp"

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"

#include "IdTypeTraits.hpp"
#include "StringViewTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    UIState::UIState(std::string_view item, ObjectPtr<User> user)
        : _item{ item }
        , _user{ getDboPtr(user) }
    {
    }

    UIState::pointer UIState::create(Session& session, std::string_view item, ObjectPtr<User> user)
    {
        return session.getDboSession()->add(std::unique_ptr<UIState>{ new UIState{ item, user } });
    }

    std::size_t UIState::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM ui_state"));
    }

    UIState::pointer UIState::find(Session& session, UIStateId settingId)
    {
        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<UIState>>("SELECT ui_s from ui_state ui_s").where("ui_s.id = ?").bind(settingId) };
        return utils::fetchQuerySingleResult(query);
    }

    UIState::pointer UIState::find(Session& session, std::string_view item, UserId userId)
    {
        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<UIState>>("SELECT ui_s from ui_state ui_s").where("ui_s.item = ?").bind(item).where("ui_s.user_id = ?").bind(userId) };
        return utils::fetchQuerySingleResult(query);
    }
} // namespace lms::db