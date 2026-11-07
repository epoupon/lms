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

#pragma once

#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>

#include "database/Object.hpp"
#include "database/objects/UIStateId.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class ISession;
    class User;

    class UIState final : public Object<UIState, UIStateId>
    {
    public:
        UIState() = default;

        static pointer create(ISession& session, std::string_view item, const ObjectPtr<User>& user);

        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, UIStateId settingId);
        static pointer find(ISession& session, std::string_view item, UserId userId);

        // Getters
        std::string_view getValue() const { return _value; };

        // Setters
        void setValue(std::string_view value) { _value = value; };

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _item, "item");
            Wt::Dbo::field(a, _value, "value");

            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

    private:
        UIState(std::string_view item, const ObjectPtr<User>& user);

        std::string _item;
        std::string _value;
        Wt::Dbo::ptr<User> _user;
    };
} // namespace lms::db
