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

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "database/objects/UIStateId.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class UIState : public detail::Object<UIState, UIStateId>
    {
    public:
        static UIState create(ISession& session, std::string_view item, UserId user);
        static void destroy(ISession& session, std::span<const UIStateId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<UIState> find(ISession& session, UIStateId settingId);
        static std::optional<UIState> find(ISession& session, std::string_view item, UserId userId);

        static void update(ISession& session, const UIState& uiState);

        // Getters
        std::string_view getValue() const { return _value; };

        // Setters
        void setValue(std::string_view value) { _value = value; };

    protected:
        UIState() = default;
        explicit UIState(std::string_view item);

        std::string _item;
        std::string _value;
    };
} // namespace lms::db
