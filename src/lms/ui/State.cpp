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

#include <array>

#include "database/ISession.hpp"
#include "database/objects/UIState.hpp"
#include "database/objects/User.hpp"

#include "LmsApplication.hpp"

namespace lms::ui::state::detail
{
    void writeValue(std::string_view item, std::string_view value)
    {
        // No UI state stored for demo user
        if (LmsApp->getUserType() == db::UserType::DEMO)
            return;

        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            std::optional<db::UIState> state{ db::UIState::find(LmsApp->getDbSession(), item, LmsApp->getUserId()) };
            if (!state)
            {
                if (LmsApp->getUserId().isValid())
                    state = db::UIState::create(LmsApp->getDbSession(), item, LmsApp->getUserId());
            }

            if (state)
            {
                state->setValue(value);
                db::UIState::update(LmsApp->getDbSession(), *state);
            }
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

            const std::optional<db::UIState> state{ db::UIState::find(LmsApp->getDbSession(), item, LmsApp->getUserId()) };
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

            if (const std::optional<db::UIState> state{ db::UIState::find(LmsApp->getDbSession(), item, LmsApp->getUserId()) })
                db::UIState::destroy(LmsApp->getDbSession(), std::array{ state->getId() });
        }
    }
} // namespace lms::ui::state::detail
