/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "database/objects/Movement.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/ISession.hpp"

#include "MovementDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "TrackDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Movement)

namespace lms::db
{
    Movement::Movement(std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
        , _number{ number }
        , _count{ count }
    {
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Movement name too long, truncated to '" << _name << "'");
    }

    dbo::Movement::Movement(std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<db::Track>& track)
        : db::Movement{ name, number, count }
        , _track{ getDboPtr(track) }
    {
    }

    Movement::pointer Movement::create(ISession& session, std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<Track>& track)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Movement>{ new dbo::Movement{ name, number, count, track } });
    }

} // namespace lms::db
