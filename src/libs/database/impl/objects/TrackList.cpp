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

#include "ClusterDbo.hpp"
#include "DboAccess.hpp"
#include "PlayListFileDbo.hpp"
#include "Session.hpp"
#include "TrackDbo.hpp"
#include "TrackListDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "objects/FilterExprCompiler.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackList)
LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackListEntry)

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

    TrackList TrackList::create(ISession& session, std::string_view name, TrackListType type)
    {
        return utils::addObject(session, std::unique_ptr<dbo::TrackList>{ new dbo::TrackList{ name, type } });
    }

    void TrackList::destroy(ISession& session, std::span<const TrackListId> ids)
    {
        utils::destroyObjects<dbo::TrackList>(session, ids);
    }

    std::size_t TrackList::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM tracklist"));
    }

    std::vector<TrackListId> TrackList::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createListQuery<TrackListId>(session, "t_l.id", params) };
        return utils::execRangeQuery<TrackListId>(query, params.range);
    }

    void TrackList::find(ISession& session, const FindParameters& params, const std::function<void(const TrackList&)>& func)
    {
        checkReadTransaction(session);
        auto query{ createListQuery<Wt::Dbo::ptr<dbo::TrackList>>(session, "t_l", params) };
        utils::forEachQueryRangeObject(query, params.range, func);
    }

    std::optional<TrackList> TrackList::find(ISession& session, TrackListId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackList>>("select t_l from tracklist t_l").where("t_l.id = ?").bind(id));
    }

    std::optional<TrackList> TrackList::find(ISession& session, PlayListFileId playListFileId)
    {
        checkReadTransaction(session);

        if (!playListFileId.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackList>>("select t_l from tracklist t_l").where("t_l.playlist_file_id = ?").bind(playListFileId));
    }

    bool TrackList::isEmpty(ISession& session, TrackListId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT EXISTS (SELECT 1 FROM tracklist_entry WHERE tracklist_id = ?)").bind(id)) == 0;
    }

    std::size_t TrackList::getTrackCount(ISession& session, TrackListId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM tracklist_entry").where("tracklist_id = ?").bind(id));
    }

    std::chrono::milliseconds TrackList::getDuration(ISession& session, TrackListId id)
    {
        checkReadTransaction(session);

        using milli = std::chrono::duration<int, std::milli>;

        return utils::fetchQuerySingleResult(getDboSession(session)->query<milli>("SELECT COALESCE(SUM(duration), 0) FROM track t INNER JOIN tracklist_entry p_e ON t.id = p_e.track_id").where("p_e.tracklist_id = ?").bind(id));
    }

    void TrackList::update(ISession& session, const TrackList& trackList)
    {
        utils::updateObject<dbo::TrackList>(session, trackList);
    }

    void TrackList::clear(ISession& session, TrackListId id)
    {
        utils::modifyObject<dbo::TrackList>(session, id)._entries.clear();
    }

    void TrackList::setLastModifiedDateTime(const Wt::WDateTime& dateTime)
    {
        _lastModifiedDateTime = utils::normalizeDateTime(dateTime);
    }

    void dbo::TrackList::applyIds()
    {
        Wt::Dbo::Session& dboSession{ *session() };
        setRelation(dboSession, _user, _userId);
        setRelation(dboSession, _playListFile, _playListFileId);
    }

    TrackListEntry::TrackListEntry(const Wt::WDateTime& dateTime)
        : _dateTime{ utils::normalizeDateTime(dateTime) }
    {
    }

    dbo::TrackListEntry::TrackListEntry(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<TrackList> tracklist, const Wt::WDateTime& dateTime)
        : db::TrackListEntry{ dateTime }
        , _track{ std::move(track) }
        , _tracklist{ std::move(tracklist) }
    {
        assert(_track);
        assert(_tracklist);
    }

    TrackListEntry TrackListEntry::create(ISession& session, TrackId track, TrackListId tracklist, const Wt::WDateTime& dateTime)
    {
        Wt::Dbo::Session& dboSession{ *getDboSession(session) };
        return utils::addObject(session, std::unique_ptr<dbo::TrackListEntry>{ new dbo::TrackListEntry{ dboSession.loadLazy<dbo::Track>(track.getValue()), dboSession.loadLazy<dbo::TrackList>(tracklist.getValue()), dateTime } });
    }

    void TrackListEntry::destroy(ISession& session, std::span<const TrackListEntryId> ids)
    {
        utils::destroyObjects<dbo::TrackListEntry>(session, ids);
    }

    std::optional<TrackListEntry> TrackListEntry::getById(ISession& session, TrackListEntryId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackListEntry>().where("id = ?").bind(id));
    }

    void TrackListEntry::find(ISession& session, const FindParameters& params, const std::function<void(const TrackListEntry&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::TrackListEntry>>("SELECT t_l_e FROM tracklist_entry t_l_e") };

        if (params.trackList.isValid())
            query.where("t_l_e.tracklist_id = ?").bind(params.trackList);
        query.orderBy("t_l_e.id");

        utils::forEachQueryRangeObject(query, params.range, func);
    }
} // namespace lms::db
