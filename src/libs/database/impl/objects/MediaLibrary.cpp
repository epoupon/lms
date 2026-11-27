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

#include "database/objects/MediaLibrary.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/String.hpp"

#include "DboAccess.hpp"
#include "MediaLibraryDbo.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::MediaLibrary)

namespace lms::db
{
    MediaLibrary::MediaLibrary(std::string_view name, const std::filesystem::path& p)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
    {
        setPath(p);
    }

    MediaLibrary MediaLibrary::create(ISession& session, std::string_view name, const std::filesystem::path& p)
    {
        return utils::addObject(session, std::unique_ptr<dbo::MediaLibrary>{ new dbo::MediaLibrary{ name, p } });
    }

    void MediaLibrary::destroy(ISession& session, std::span<const MediaLibraryId> ids)
    {
        utils::destroyObjects<dbo::MediaLibrary>(session, ids);
    }

    std::size_t MediaLibrary::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM media_library"));
    }

    std::optional<MediaLibrary> MediaLibrary::find(ISession& session, MediaLibraryId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::MediaLibrary>().where("id = ?").bind(id));
    }

    std::optional<MediaLibrary> MediaLibrary::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::MediaLibrary>().where("name = ?").bind(name));
    }

    std::optional<MediaLibrary> MediaLibrary::find(ISession& session, const std::filesystem::path& p)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::MediaLibrary>().where("path = ?").bind(p));
    }

    void MediaLibrary::find(ISession& session, const std::function<void(const MediaLibrary&)>& func)
    {
        checkReadTransaction(session);

        utils::forEachQueryObject(getDboSession(session)->find<dbo::MediaLibrary>(), func);
    }

    bool MediaLibrary::isEmpty(ISession& session, MediaLibraryId id)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<bool>("SELECT EXISTS (SELECT 1 FROM track WHERE media_library_id = ? LIMIT 1) AS is_media_library_empty").bind(id) };
        return !utils::fetchQuerySingleResult(query);
    }

    void MediaLibrary::setPath(const std::filesystem::path& p)
    {
        assert(p.is_absolute());
        if (!p.has_filename() && p.has_parent_path())
            _path = p.parent_path();
        else
            _path = p;
    }

    void MediaLibrary::update(ISession& session, const MediaLibrary& mediaLibrary)
    {
        utils::updateObject<dbo::MediaLibrary>(session, mediaLibrary);
    }
} // namespace lms::db
