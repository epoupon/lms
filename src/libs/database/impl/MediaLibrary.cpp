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

#include "database/MediaLibrary.hpp"

#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"

#include "IdTypeTraits.hpp"
#include "PathTraits.hpp"
#include "StringViewTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    MediaLibrary::MediaLibrary(const std::filesystem::path& p, std::string_view name)
        : _path{ p }
        , _name{ std::string{ name, 0, maxNameLength } }
    {
    }

    MediaLibrary::pointer MediaLibrary::create(Session& session, const std::filesystem::path& p, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<MediaLibrary>{ new MediaLibrary{ p, name } });
    }

    std::size_t MediaLibrary::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM media_library"));
    }

    MediaLibrary::pointer MediaLibrary::find(Session& session, MediaLibraryId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<MediaLibrary>().where("id = ?").bind(id));
    }

    MediaLibrary::pointer MediaLibrary::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<MediaLibrary>().where("name = ?").bind(name));
    }

    MediaLibrary::pointer MediaLibrary::find(Session& session, const std::filesystem::path& p)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<MediaLibrary>().where("path = ?").bind(p));
    }

    void MediaLibrary::find(Session& session, std::function<void(const MediaLibrary::pointer&)> func)
    {
        session.checkReadTransaction();

        utils::forEachQueryResult(session.getDboSession()->find<MediaLibrary>(), [&](const MediaLibrary::pointer& mediaLibrary) {
            func(mediaLibrary);
        });
    }
} // namespace lms::db
