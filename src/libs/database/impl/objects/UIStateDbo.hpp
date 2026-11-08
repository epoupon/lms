/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/UIState.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class User;

    class UIState final : public db::UIState, public Wt::Dbo::Dbo<UIState>
    {
    public:
        UIState() = default;
        using db::UIState::UIState;

        UIState(std::string_view item, const ObjectPtr<db::User>& user);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _item, "item");
            Wt::Dbo::field(a, _value, "value");

            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<User> _user;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<UIState>
    {
        using type = dbo::UIState;
    };
} // namespace lms::db
