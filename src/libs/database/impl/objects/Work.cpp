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

#include "database/objects/Work.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/UUIDTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Work)

namespace lms::db
{
    Work::Work(std::string_view name, const std::optional<core::UUID>& mbid)
        : _mbid{ mbid }
    {
        setName(name);
    }

    void Work::setName(std::string_view name)
    {
        _name = core::stringUtils::utf8Truncate(name, maxNameLength);
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Work name too long, truncated to '" << _name << "'");
    }

    Work::pointer Work::create(ISession& session, std::string_view name, const std::optional<core::UUID>& mbid)
    {
        return utils::addObject(session, std::unique_ptr<Work>{ new Work{ name, mbid } });
    }

    void Work::destroy(ISession& session, std::span<const WorkId> ids)
    {
        utils::destroyObjects<Work>(session, ids);
    }

    Work::pointer Work::find(ISession& session, WorkId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<Work>().where("id = ?").bind(id));
    }

    Work::pointer Work::find(ISession& session, const core::UUID& mbid)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<Work>().where("mbid = ?").bind(mbid));
    }

    Work::pointer Work::find(ISession& session, ReleaseId releaseId, std::string_view name)
    {
        checkReadTransaction(session);

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        auto query{
            getDboSession(session)->query<Wt::Dbo::ptr<Work>>("SELECT w FROM work w")
                // clang-format off
                .join("track_work t_w ON t_w.work_id = w.id")
                .join("track t ON t.id = t_w.track_id")
                .where("t.release_id = ?").bind(releaseId)
                .where("w.name = ?").bind(std::string{ name })
                .where("w.mbid IS NULL")
                .groupBy("w.id")
            // clang-format on
        };

        return utils::fetchQuerySingleResult(query);
    }

    std::vector<WorkId> Work::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);
        auto query{ getDboSession(session)->query<WorkId>("SELECT w.id FROM work w WHERE NOT EXISTS (SELECT 1 FROM track_work t_w WHERE t_w.work_id = w.id)") };
        return utils::execRangeQuery<WorkId>(query, range);
    }

} // namespace lms::db
