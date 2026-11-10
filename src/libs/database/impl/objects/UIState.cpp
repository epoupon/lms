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

#include "DboAccess.hpp"
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

    dbo::UIState::UIState(std::string_view item, Wt::Dbo::ptr<User> user)
        : db::UIState{ item }
        , _user{ std::move(user) }
    {
    }

    UIState UIState::create(ISession& session, std::string_view item, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::UIState>{ new dbo::UIState{ item, getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void UIState::destroy(ISession& session, std::span<const UIStateId> ids)
    {
        utils::destroyObjects<dbo::UIState>(session, ids);
    }

    std::size_t UIState::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM ui_state"));
    }

    std::optional<UIState> UIState::find(ISession& session, UIStateId settingId)
    {
        checkReadTransaction(session);

        if (!settingId.isValid())
            return std::nullopt;

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::UIState>>("SELECT ui_s from ui_state ui_s").where("ui_s.id = ?").bind(settingId) };
        return utils::fetchQuerySingleObject(query);
    }

    std::optional<UIState> UIState::find(ISession& session, std::string_view item, UserId userId)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::UIState>>("SELECT ui_s from ui_state ui_s").where("ui_s.item = ?").bind(item).where("ui_s.user_id = ?").bind(userId) };
        return utils::fetchQuerySingleObject(query);
    }

    void UIState::update(ISession& session, const UIState& uiState)
    {
        utils::updateObject<dbo::UIState>(session, uiState);
    }
} // namespace lms::db