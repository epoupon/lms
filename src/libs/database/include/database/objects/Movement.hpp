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

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "database/objects/MovementId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class Movement : public detail::Object<Movement, MovementId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        static Movement create(ISession& session, std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, TrackId track);

        static void destroy(ISession& session, std::span<const MovementId> ids);
        static void find(ISession& session, TrackId trackId, const std::function<void(const Movement&)>& func); // in creation order

        std::string_view getName() const { return _name; }
        std::optional<std::size_t> getNumber() const { return _number; }
        std::optional<std::size_t> getCount() const { return _count; }

    protected:
        Movement() = default;
        Movement(std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count);

        std::string _name;
        std::optional<int> _number;
        std::optional<int> _count;
    };

} // namespace lms::db
