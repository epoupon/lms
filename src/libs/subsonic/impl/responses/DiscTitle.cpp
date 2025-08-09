/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "responses/DiscTitle.hpp"

#include "database/objects/Medium.hpp"

namespace lms::api::subsonic
{
    Response::Node createDiscTitle(const db::ObjectPtr<db::Medium>& medium)
    {
        Response::Node discTitleNode;

        discTitleNode.setAttribute("disc", medium->getPosition() ? *medium->getPosition() : 0);
        discTitleNode.setAttribute("title", medium->getName());

        return discTitleNode;
    }
} // namespace lms::api::subsonic