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

#include "database/Session.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "SqlQuery.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackList)
DBO_INSTANTIATE_TEMPLATES(lms::db::TrackListEntry)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const TrackList::FindParameters& params)
        {
            assert(!params.user.isValid() || !params.excludedUser.isValid());

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM tracklist t_l") };

            if (!params.filters.clusters.empty()
                || params.filters.mediaLibrary.isValid()
                || params.filters.label.isValid())
            {
                query.join("tracklist_entry t_l_e ON t_l_e.tracklist_id = t_l.id");
                query.groupBy("t_l.id");
            }

            for (std::string_view keyword : params.keywords)
                query.where("t_l.name LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeLikeKeyword(keyword) + "%");

            if (params.filters.mediaLibrary.isValid()
                || params.filters.label.isValid()
                || params.filters.releaseType.isValid())
            {
                query.join("track t ON t.id = t_l_e.track_id");

                if (params.filters.mediaLibrary.isValid())
                    query.where("t.media_library_id = ?").bind(params.filters.mediaLibrary);

                if (params.filters.label.isValid())
                {
                    query.join("release_label r_l ON r_l.release_id = t.release_id");
                    query.where("r_l.label_id = ?").bind(params.filters.label);
                }

                if (params.filters.releaseType.isValid())
                {
                    query.join("release_release_type r_r_t ON r_r_t.release_id = t.release_id");
                    query.where("r_r_t.release_type_id = ?").bind(params.filters.releaseType);
                }
            }

            if (params.user.isValid())
                query.where("t_l.user_id = ?").bind(params.user);
            else if (params.excludedUser.isValid())
                query.where("t_l.user_id <> ? OR t_l.user_id IS NULL").bind(params.excludedUser);

            if (params.type)
                query.where("t_l.type = ?").bind(*params.type);

            if (params.visibility)
                query.where("t_l.visibility = ?").bind(*params.visibility);

            if (!params.filters.clusters.empty())
            {
                std::ostringstream oss;
                oss << "t_l_e.track_id IN (SELECT DISTINCT t.id FROM track t"
                       " INNER JOIN track_cluster t_c ON t_c.track_id = t.id"
                       " INNER JOIN cluster c ON c.id = t_c.cluster_id";

                WhereClause clusterClause;
                for (const ClusterId clusterId : params.filters.clusters)
                {
                    clusterClause.Or(WhereClause("c.id = ?"));
                    query.bind(clusterId);
                }

                oss << " " << clusterClause.get();
                oss << " GROUP BY t.id HAVING COUNT(*) = " << params.filters.clusters.size() << ")";

                query.where(oss.str());
            }

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

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, const TrackList::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, TrackListId>)
                itemToSelect = "t_l.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<TrackList>>)
                itemToSelect = "t_l";
            else
                static_assert("Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    TrackList::TrackList(std::string_view name, TrackListType type)
        : _name{ name }
        , _type{ type }
        , _creationDateTime{ utils::normalizeDateTime(Wt::WDateTime::currentDateTime()) }
        , _lastModifiedDateTime{ utils::normalizeDateTime(Wt::WDateTime::currentDateTime()) }
    {
    }

    TrackList::pointer TrackList::create(Session& session, std::string_view name, TrackListType type)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackList>{ new TrackList{ name, type } });
    }

    std::size_t TrackList::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM tracklist"));
    }

    TrackList::pointer TrackList::find(Session& session, std::string_view name, TrackListType type, UserId userId)
    {
        session.checkReadTransaction();
        assert(userId.isValid());

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<TrackList>>("select t_l from tracklist t_l").where("t_l.name = ?").bind(name).where("t_l.type = ?").bind(type).where("t_l.user_id = ?").bind(userId));
    }

    RangeResults<TrackListId> TrackList::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<TrackListId>(session, params) };
        return utils::execRangeQuery<TrackListId>(query, params.range);
    }

    void TrackList::find(Session& session, const FindParameters& params, const std::function<void(const TrackList::pointer&)>& func)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<TrackList>>(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    TrackList::pointer TrackList::find(Session& session, TrackListId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<TrackList>>("select t_l from tracklist t_l").where("t_l.id = ?").bind(id));
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
        if (!entries.results.empty())
            res = entries.results.front();

        return res;
    }

    RangeResults<ObjectPtr<TrackListEntry>> TrackList::getEntries(std::optional<Range> range) const
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

    std::vector<std::vector<Cluster::pointer>> TrackList::getClusterGroups(const std::vector<ClusterTypeId>& clusterTypeIds, std::size_t size) const
    {
        assert(session());
        std::vector<std::vector<Cluster::pointer>> res;

        if (clusterTypeIds.empty())
            return res;

        auto query{ session()->query<Wt::Dbo::ptr<Cluster>>("SELECT c from cluster c") };

        query.join("track t ON c.id = t_c.cluster_id")
            .join("track_cluster t_c ON t_c.track_id = t.id")
            .join("cluster_type c_type ON c.cluster_type_id = c_type.id")
            .join("tracklist_entry t_l_e ON t_l_e.track_id = t.id")
            .join("tracklist t_l ON t_l.id = t_l_e.tracklist_id")
            .where("t_l.id = ?")
            .bind(getId());

        {
            std::ostringstream oss;
            oss << "c_type.id IN (";
            bool first{ true };
            for (ClusterTypeId clusterTypeId : clusterTypeIds)
            {
                if (!first)
                    oss << ", ";
                oss << "?";
                query.bind(clusterTypeId);
                first = false;
            }
            oss << ")";
            query.where(oss.str());
        }
        query.groupBy("c.id");
        query.orderBy("COUNT(c.id) DESC");

        std::map<ClusterTypeId, std::vector<Cluster::pointer>> clustersByType;
        utils::forEachQueryResult(query, [&](const Cluster::pointer& cluster) {
            if (clustersByType[cluster->getType()->getId()].size() < size)
                clustersByType[cluster->getType()->getId()].push_back(cluster);
        });

        for (const auto& [clusterTypeId, clusters] : clustersByType)
            res.push_back(clusters);

        return res;
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

    std::vector<TrackId> TrackList::getTrackIds() const
    {
        assert(session());

        auto query{ session()->query<TrackId>("SELECT p_e.track_id from tracklist_entry p_e INNER JOIN tracklist p ON p_e.tracklist_id = p.id").where("p.id = ?").bind(getId()) };

        return utils::fetchQueryResults(query);
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

    TrackListEntry::pointer TrackListEntry::create(Session& session, ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackListEntry>{ new TrackListEntry{ track, tracklist, dateTime } });
    }

    TrackListEntry::pointer TrackListEntry::getById(Session& session, TrackListEntryId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<TrackListEntry>().where("id = ?").bind(id));
    }

    void TrackListEntry::find(Session& session, const FindParameters& params, const std::function<void(const TrackListEntry::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<TrackListEntry>>("SELECT t_l_e FROM tracklist_entry t_l_e") };

        if (params.trackList.isValid())
            query.where("t_l_e.tracklist_id = ?").bind(params.trackList);
        query.orderBy("t_l_e.id");

        utils::forEachQueryRangeResult(query, params.range, func);
    }

} // namespace lms::db
