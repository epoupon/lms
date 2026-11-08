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

#include "database/objects/Mood.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "core/Utils.hpp"

#include "ArtistDbo.hpp"
#include "ArtworkDbo.hpp"
#include "ClusterDbo.hpp"
#include "DirectoryDbo.hpp"
#include "GenreDbo.hpp"
#include "GroupingDbo.hpp"
#include "LanguageDbo.hpp"
#include "MediaLibraryDbo.hpp"
#include "MediumDbo.hpp"
#include "MoodDbo.hpp"
#include "MovementDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "TrackArtistLinkDbo.hpp"
#include "TrackDbo.hpp"
#include "TrackEmbeddedImageDbo.hpp"
#include "TrackEmbeddedImageLinkDbo.hpp"
#include "TrackLyricsDbo.hpp"
#include "Utils.hpp"
#include "WorkDbo.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::Mood)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(ISession& session, std::string_view itemToSelect, const Mood::FindParameters& params)
        {
            checkReadTransaction(session);

            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM mood m") };

            if (params.artist.isValid() || params.track.isValid() || params.release.isValid()
                || params.sortMethod == MoodSortMethod::TrackCountDesc)
                query.join("track_mood t_m ON t_m.mood_id = m.id");

            if (params.track.isValid())
                query.where("t_m.track_id = ?").bind(params.track);

            if (params.release.isValid() || params.artist.isValid())
                query.join("track t ON t.id = t_m.track_id");

            if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);

            if (params.artist.isValid())
            {
                query.join("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                query.where("t_a_l.artist_id = ?").bind(params.artist);
            }

            switch (params.sortMethod)
            {
            case MoodSortMethod::None:
                break;
            case MoodSortMethod::Name:
                query.orderBy("m.name COLLATE NOCASE");
                break;
            case MoodSortMethod::TrackCountDesc:
                query.orderBy("COUNT(t_m.track_id) DESC");
                break;
            }

            // track_mood has a UNIQUE constraint on (track_id, mood_id), so no duplicates when filtering by track
            if (!params.track.isValid())
                query.groupBy("m.id");

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(ISession& session, const Mood::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, MoodId>)
                itemToSelect = "m.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<dbo::Mood>>)
                itemToSelect = "m";
            else
                static_assert(core::utils::dependent_false_v<ResultType>, "Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Mood::Mood(std::string_view name)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
    {
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Mood name too long, truncated to '" << _name << "'");
    }

    Mood::pointer Mood::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::Mood>{ new dbo::Mood{ name } });
    }

    void Mood::destroy(ISession& session, std::span<const MoodId> ids)
    {
        utils::destroyObjects<dbo::Mood>(session, ids);
    }

    std::size_t Mood::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM mood"));
    }

    std::vector<MoodId> Mood::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createQuery<MoodId>(session, params) };
        return utils::execRangeQuery<MoodId>(query, params.range);
    }

    std::vector<Mood::pointer> Mood::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createQuery<Wt::Dbo::ptr<dbo::Mood>>(session, params) };
        return utils::execRangeQuery<Mood::pointer>(query, params.range);
    }

    void Mood::find(ISession& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);
        auto query{ createQuery<Wt::Dbo::ptr<dbo::Mood>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    Mood::pointer Mood::find(ISession& session, MoodId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::Mood>().where("id = ?").bind(id));
    }

    Mood::pointer Mood::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::Mood>().where("name = ?").bind(name));
    }

    std::vector<MoodId> Mood::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);
        auto query{ getDboSession(session)->query<MoodId>("SELECT m.id FROM mood m WHERE NOT EXISTS (SELECT 1 FROM track_mood t_m WHERE t_m.mood_id = m.id)") };
        return utils::execRangeQuery<MoodId>(query, range);
    }

} // namespace lms::db
