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

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/Work.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Mood)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const Mood::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM mood m") };

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
        Wt::Dbo::Query<ResultType> createQuery(Session& session, const Mood::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, MoodId>)
                itemToSelect = "m.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<Mood>>)
                itemToSelect = "m";
            else
                static_assert("Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Mood::Mood(std::string_view name)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
    {
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Mood name too long, truncated to '" << _name << "'");
    }

    Mood::pointer Mood::create(Session& session, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<Mood>{ new Mood{ name } });
    }

    std::size_t Mood::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM mood"));
    }

    std::vector<MoodId> Mood::findIds(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<MoodId>(session, params) };
        return utils::execRangeQuery<MoodId>(query, params.range);
    }

    std::vector<Mood::pointer> Mood::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<Mood>>(session, params) };
        return utils::execRangeQuery<Mood::pointer>(query, params.range);
    }

    void Mood::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<Mood>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    Mood::pointer Mood::find(Session& session, MoodId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<Mood>().where("id = ?").bind(id));
    }

    Mood::pointer Mood::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        return utils::fetchQuerySingleResult(session.getDboSession()->find<Mood>().where("name = ?").bind(name));
    }

    std::vector<MoodId> Mood::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ session.getDboSession()->query<MoodId>("SELECT m.id FROM mood m WHERE NOT EXISTS (SELECT 1 FROM track_mood t_m WHERE t_m.mood_id = m.id)") };
        return utils::execRangeQuery<MoodId>(query, range);
    }

} // namespace lms::db
