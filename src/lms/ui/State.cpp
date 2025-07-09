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

#include "State.hpp"

#include "database/Session.hpp"
#include "database/objects/UIState.hpp"
#include "database/objects/User.hpp"

#include "LmsApplication.hpp"

namespace lms::ui::state::details
{
    void writeValue(std::string_view item, std::string_view value)
    {
        // No UI state stored for demo user
        if (LmsApp->getUserType() == db::UserType::DEMO)
            return;

        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            db::UIState::pointer state{ db::UIState::find(LmsApp->getDbSession(), item, LmsApp->getUserId()) };
            if (!state)
            {
                if (db::User::pointer user{ LmsApp->getUser() })
                    state = LmsApp->getDbSession().create<db::UIState>(item, user);
            }

            if (state)
                state.modify()->setValue(value);
        }
    }

    std::optional<std::string> readValue(std::string_view item)
    {
        std::optional<std::string> res;

        // No UI state stored for demo user
        if (LmsApp->getUserType() == db::UserType::DEMO)
            return res;

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::UIState::pointer state{ db::UIState::find(LmsApp->getDbSession(), item, LmsApp->getUserId()) };
            if (state)
                res = state->getValue();
        }

        return res;
    }

    void eraseValue(std::string_view item)
    {
        // No UI state stored for demo user
        if (LmsApp->getUserType() == db::UserType::DEMO)
            return;

        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            db::UIState::pointer state{ db::UIState::find(LmsApp->getDbSession(), item, LmsApp->getUserId()) };
            if (state)
                state.remove();
        }
    }

} // namespace lms::ui::state::details
