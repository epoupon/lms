/*
 * Copyright (C) 2014 Emeric Poupon
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
#include "database/objects/TrackList.hpp"

#include <cassert>

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"
#include "database/objects/Work.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackList)
DBO_INSTANTIATE_TEMPLATES(lms::db::TrackListEntry)

namespace lms::db
{
    namespace
    {
        SqlFragment createParamsConditions(const TrackList::FindParameters& params)
        {
            assert(!params.user.isValid() || !params.excludedUser.isValid());

            SqlFragment res;

            for (const std::string_view keyword : params.keywords)
                res.combineWithAnd({ "t_l.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'", { "%" + utils::escapeForLikeKeyword(keyword) + "%" } });

            if (params.user.isValid())
                res.combineWithAnd({ "t_l.user_id = ?", { params.user.getValue() } });
            else if (params.excludedUser.isValid())
                res.combineWithAnd({ "t_l.user_id <> ? OR t_l.user_id IS NULL", { params.excludedUser.getValue() } });

            if (params.type)
                res.combineWithAnd({ "t_l.type = ?", { static_cast<int>(*params.type) } });

            if (params.visibility)
                res.combineWithAnd({ "t_l.visibility = ?", { static_cast<int>(*params.visibility) } });

            if (params.filter)
            {
                SqlFragment filter{ "t_l.id IN (SELECT t_l_e.tracklist_id FROM tracklist_entry t_l_e WHERE " };
                filter.append(createMatchingTracksCondition("t_l_e.track_id", *params.filter, params.filterUser));
                filter.append(")");
                res.combineWithAnd(filter);
            }

            return res;
        }

        template<typename ResultType>
        void applySortMethod(Wt::Dbo::Query<ResultType>& query, const TrackList::FindParameters& params)
        {
            switch (params.sortMethod)
            {
            case TrackListSortMethod::None:
                break;
            case TrackListSortMethod::Name:
                query.orderBy("t_l.name COLLATE NOCASE");
                break;
            case TrackListSortMethod::LastModifiedDesc:
                query.orderBy("t_l.last_modified_date_time DESC");
                break;
            }
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createListQuery(ISession& session, std::string_view selectedItem, const TrackList::FindParameters& params)
        {
            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ selectedItem } + " FROM tracklist t_l") };
            applyWhereFragment(query, createParamsConditions(params));
            applySortMethod(query, params);

            return query;
        }
    } // namespace

    TrackList::TrackList(std::string_view name, TrackListType type)
        : _name{ name }
        , _type{ type }
        , _creationDateTime{ utils::normalizeDateTime(Wt::WDateTime::currentDateTime()) }
        , _lastModifiedDateTime{ utils::normalizeDateTime(Wt::WDateTime::currentDateTime()) }
    {
    }

    TrackList::pointer TrackList::create(ISession& session, std::string_view name, TrackListType type)
    {
        return utils::addObject(session, std::unique_ptr<TrackList>{ new TrackList{ name, type } });
    }

    std::size_t TrackList::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM tracklist"));
    }

    TrackList::pointer TrackList::find(ISession& session, std::string_view name, TrackListType type, UserId userId)
    {
        checkReadTransaction(session);
        assert(userId.isValid());

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<TrackList>>("select t_l from tracklist t_l").where("t_l.name = ?").bind(name).where("t_l.type = ?").bind(type).where("t_l.user_id = ?").bind(userId));
    }

    std::vector<TrackListId> TrackList::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createListQuery<TrackListId>(session, "t_l.id", params) };
        return utils::execRangeQuery<TrackListId>(query, params.range);
    }

    void TrackList::find(ISession& session, const FindParameters& params, const std::function<void(const TrackList::pointer&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createListQuery<Wt::Dbo::ptr<TrackList>>(session, "t_l", params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    TrackList::pointer TrackList::find(ISession& session, TrackListId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<TrackList>>("select t_l from tracklist t_l").where("t_l.id = ?").bind(id));
    }

    bool TrackList::isEmpty() const
    {
        return _entries.empty();
    }

    std::size_t TrackList::getCount() const
    {
        return _entries.size();
    }

    TrackListEntry::pointer TrackList::getEntry(std::size_t pos) const
    {
        TrackListEntry::pointer res;

        auto entries = getEntries(Range{ pos, 1 });
        if (!entries.empty())
            res = entries.front();

        return res;
    }

    std::vector<ObjectPtr<TrackListEntry>> TrackList::getEntries(std::optional<Range> range) const
    {
        assert(session());

        auto query{ session()->find<TrackListEntry>().where("tracklist_id = ?").bind(getId()).orderBy("id") };

        return utils::execRangeQuery<TrackListEntry::pointer>(query, range);
    }

    TrackListEntry::pointer TrackList::getEntryByTrackAndDateTime(ObjectPtr<Track> track, const Wt::WDateTime& dateTime) const
    {
        assert(session());

        return utils::fetchQuerySingleResult(session()->find<TrackListEntry>().where("tracklist_id = ?").bind(getId()).where("track_id = ?").bind(track->getId()).where("date_time = ?").bind(utils::normalizeDateTime(dateTime)));
    }

    std::vector<Cluster::pointer> TrackList::getClusters() const
    {
        assert(session());

        const auto query{ session()->query<Wt::Dbo::ptr<Cluster>>("SELECT c from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id").where("p.id = ?").bind(getId()).groupBy("c.id").orderBy("COUNT(c.id) DESC") };

        return utils::fetchQueryResults<Cluster::pointer>(query);
    }

    std::vector<Track::pointer> TrackList::getSimilarTracks(std::optional<std::size_t> offset, std::optional<std::size_t> size) const
    {
        assert(session());

        auto query{ session()->query<Wt::Dbo::ptr<Track>>(
                                 "SELECT t FROM track t"
                                 " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                                 " WHERE "
                                 " (t_c.cluster_id IN (SELECT DISTINCT c.id from cluster c INNER JOIN track t ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id INNER JOIN tracklist_entry p_e ON p_e.track_id = t.id INNER JOIN tracklist p ON p.id = p_e.tracklist_id WHERE p.id = ?)"
                                 " AND t.id NOT IN (SELECT tracklist_t.id FROM track tracklist_t INNER JOIN tracklist_entry t_e ON t_e.track_id = tracklist_t.id WHERE t_e.tracklist_id = ?))")
                        .bind(getId())
                        .bind(getId())
                        .groupBy("t.id")
                        .orderBy("COUNT(*) DESC, RANDOM()")
                        .limit(size ? static_cast<int>(*size) : -1)
                        .offset(offset ? static_cast<int>(*offset) : -1) };

        return utils::fetchQueryResults<Track::pointer>(query);
    }

    std::vector<TrackId> TrackList::getTrackIds(std::optional<Range> range) const
    {
        assert(session());

        auto query{ session()->query<TrackId>("SELECT track_id FROM tracklist_entry").where("tracklist_id = ?").bind(getId()).orderBy("id") };

        return utils::execRangeQuery<TrackId>(query, range);
    }

    void TrackList::visitTracks(const std::function<void(const ObjectPtr<Track>&)>& func) const
    {
        assert(session());

        auto query{ session()->query<Wt::Dbo::ptr<Track>>("SELECT t FROM track t").join("tracklist_entry t_l_e ON t_l_e.track_id = t.id").where("t_l_e.tracklist_id = ?").bind(getId()).orderBy("t_l_e.id") };

        utils::forEachQueryResult(query, [&](const Track::pointer& track) {
            func(track);
        });
    }

    std::chrono::milliseconds TrackList::getDuration() const
    {
        assert(session());

        using milli = std::chrono::duration<int, std::milli>;

        return utils::fetchQuerySingleResult(session()->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t INNER JOIN tracklist_entry p_e ON t.id = p_e.track_id").where("p_e.tracklist_id = ?").bind(getId()));
    }

    void TrackList::setLastModifiedDateTime(const Wt::WDateTime& dateTime)
    {
        _lastModifiedDateTime = utils::normalizeDateTime(dateTime);
    }

    TrackListEntry::TrackListEntry(ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime)
        : _dateTime{ utils::normalizeDateTime(dateTime) }
        , _track{ getDboPtr(track) }
        , _tracklist{ getDboPtr(tracklist) }
    {
        assert(track);
        assert(tracklist);
    }

    TrackListEntry::pointer TrackListEntry::create(ISession& session, ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime)
    {
        return utils::addObject(session, std::unique_ptr<TrackListEntry>{ new TrackListEntry{ track, tracklist, dateTime } });
    }

    TrackListEntry::pointer TrackListEntry::getById(ISession& session, TrackListEntryId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackListEntry>().where("id = ?").bind(id));
    }

    void TrackListEntry::find(ISession& session, const FindParameters& params, const std::function<void(const TrackListEntry::pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<TrackListEntry>>("SELECT t_l_e FROM tracklist_entry t_l_e") };

        if (params.trackList.isValid())
            query.where("t_l_e.tracklist_id = ?").bind(params.trackList);
        query.orderBy("t_l_e.id");

        utils::forEachQueryRangeResult(query, params.range, func);
    }

} // namespace lms::db
