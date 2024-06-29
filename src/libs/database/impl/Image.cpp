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

#include "database/Image.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Artist.hpp"
#include "database/Session.hpp"

#include "IdTypeTraits.hpp"
#include "PathTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    Image::Image(const std::filesystem::path& p)
        : _path{ p }
    {
    }

    Image::pointer Image::create(Session& session, const std::filesystem::path& p)
    {
        return session.getDboSession()->add(std::unique_ptr<Image>{ new Image{ p } });
    }

    std::size_t Image::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM image"));
    }

    Image::pointer Image::find(Session& session, ImageId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<Image>().where("id = ?").bind(id));
    }
} // namespace lms::db
