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

#include "database/objects/Genre.hpp"

#include <Wt/Dbo/Impl.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
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
#include "database/objects/Work.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Genre)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const Genre::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM genre g") };

            if (params.artist.isValid() || params.track.isValid() || params.release.isValid()
                || params.sortMethod == GenreSortMethod::TrackCountDesc)
                query.join("track_genre t_g ON t_g.genre_id = g.id");

            if (params.track.isValid())
                query.where("t_g.track_id = ?").bind(params.track);

            if (params.release.isValid() || params.artist.isValid())
                query.join("track t ON t.id = t_g.track_id");

            if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);

            if (params.artist.isValid())
            {
                query.join("track_artist_link t_a_l ON t_a_l.track_id = t.id");
                query.where("t_a_l.artist_id = ?").bind(params.artist);
            }

            switch (params.sortMethod)
            {
            case GenreSortMethod::None:
                break;
            case GenreSortMethod::Name:
                query.orderBy("g.name COLLATE NOCASE");
                break;
            case GenreSortMethod::TrackCountDesc:
                query.orderBy("COUNT(t_g.track_id) DESC");
                break;
            }

            // track_genre has a UNIQUE constraint on (track_id, genre_id), so no duplicates when filtering by track
            if (!params.track.isValid())
                query.groupBy("g.id");

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, const Genre::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, GenreId>)
                itemToSelect = "g.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<Genre>>)
                itemToSelect = "g";
            else
                static_assert("Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Genre::Genre(std::string_view name)
        : _name{ core::stringUtils::utf8Truncate(name, maxNameLength) }
    {
        LMS_LOG_IF(DB, WARNING, name.size() > maxNameLength, "Genre name too long, truncated to '" << _name << "'");
    }

    Genre::pointer Genre::create(Session& session, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<Genre>{ new Genre{ name } });
    }

    std::size_t Genre::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM genre"));
    }

    std::vector<GenreId> Genre::findIds(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<GenreId>(session, params) };
        return utils::execRangeQuery<GenreId>(query, params.range);
    }

    std::vector<Genre::pointer> Genre::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<Genre>>(session, params) };
        return utils::execRangeQuery<Genre::pointer>(query, params.range);
    }

    void Genre::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<Genre>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    Genre::pointer Genre::find(Session& session, GenreId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<Genre>().where("id = ?").bind(id));
    }

    Genre::pointer Genre::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        name = core::stringUtils::utf8Truncate(name, maxNameLength);

        return utils::fetchQuerySingleResult(session.getDboSession()->find<Genre>().where("name = ?").bind(name));
    }

    std::vector<GenreId> Genre::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ session.getDboSession()->query<GenreId>("SELECT g.id FROM genre g WHERE NOT EXISTS (SELECT 1 FROM track_genre t_g WHERE t_g.genre_id = g.id)") };
        return utils::execRangeQuery<GenreId>(query, range);
    }

    std::size_t Genre::computeTrackCount(Session& session, GenreId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(t.id) FROM track t INNER JOIN track_genre t_g ON t_g.track_id = t.id").where("t_g.genre_id = ?").bind(id));
    }

    std::size_t Genre::computeReleaseCount(Session& session, GenreId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(DISTINCT t.release_id) FROM track t INNER JOIN track_genre t_g ON t_g.track_id = t.id").where("t_g.genre_id = ?").bind(id));
    }

} // namespace lms::db
