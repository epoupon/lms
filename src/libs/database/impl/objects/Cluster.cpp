/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

#include "database/objects/Cluster.hpp"

#include <Wt/Dbo/Impl.h>

#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
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
#include "database/objects/Work.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::Cluster)
DBO_INSTANTIATE_TEMPLATES(lms::db::ClusterType)

namespace lms::db
{
    namespace
    {
        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(ISession& session, std::string_view itemToSelect, const Cluster::FindParameters& params)
        {
            checkReadTransaction(session);

            auto query{ getDboSession(session)->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM cluster c") };

            if (params.track.isValid() || params.release.isValid() || params.artist.isValid() || params.trackList.isValid()
                || params.sortMethod == ClusterSortMethod::TrackCountDesc)
                query.join("track_cluster t_c ON t_c.cluster_id = c.id");

            if (!params.clusterTypeName.empty())
                query.join("cluster_type c_t ON c_t.id = c.cluster_type_id");

            if (params.track.isValid())
                query.where("t_c.track_id = ?").bind(params.track);

            if (params.release.isValid())
            {
                query.join("track t ON t.id = t_c.track_id");
                query.where("t.release_id = ?").bind(params.release);
            }

            // An artist can be linked several times to a track (once per link type), and a track can be several times in a track list:
            // membership tests do not duplicate the tracks
            if (params.artist.isValid())
                query.where("t_c.track_id IN (SELECT t_a_l.track_id FROM track_artist_link t_a_l WHERE t_a_l.artist_id = ?)").bind(params.artist);

            if (params.trackList.isValid())
                query.where("t_c.track_id IN (SELECT t_l_e.track_id FROM tracklist_entry t_l_e WHERE t_l_e.tracklist_id = ?)").bind(params.trackList);

            assert(!params.clusterType.isValid() || params.clusterTypeName.empty());
            if (params.clusterType.isValid())
                query.where("c.cluster_type_id = ?").bind(params.clusterType);
            else if (!params.clusterTypeName.empty())
                query.where("c_t.name = ?").bind(params.clusterTypeName);

            switch (params.sortMethod)
            {
            case ClusterSortMethod::None:
                break;
            case ClusterSortMethod::Name:
                query.orderBy("c.name COLLATE NOCASE");
                break;
            case ClusterSortMethod::TrackCountDesc:
                query.orderBy("COUNT(*) DESC");
                break;
            }

            // track_cluster has a UNIQUE constraint on (track_id, cluster_id), so no duplicates can occur when filtering by track
            assert(!params.track.isValid() || params.sortMethod != ClusterSortMethod::TrackCountDesc);
            if (!params.track.isValid())
                query.groupBy("c.id");

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(ISession& session, const Cluster::FindParameters& params)
        {
            std::string_view itemToSelect;

            if constexpr (std::is_same_v<ResultType, ClusterId>)
                itemToSelect = "c.id";
            else if constexpr (std::is_same_v<ResultType, Wt::Dbo::ptr<Cluster>>)
                itemToSelect = "c";
            else
                static_assert("Unhandled type");

            return createQuery<ResultType>(session, itemToSelect, params);
        }
    } // namespace

    Cluster::Cluster(const ObjectPtr<ClusterType>& type, std::string_view name)
        : _name{ name }
        , _clusterType{ getDboPtr(type) }
    {
        // As we use the name to uniquely identify clusters and cluster types, we must throw (and not truncate)
        if (name.size() > maxNameLength)
            throw Exception{ "Cluster name is too long: " + std::string{ name } + "'" };
    }

    Cluster::pointer Cluster::create(ISession& session, const ObjectPtr<ClusterType>& type, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<Cluster>{ new Cluster{ type, name } });
    }

    void Cluster::destroy(ISession& session, std::span<const ClusterId> ids)
    {
        utils::destroyObjects<Cluster>(session, ids);
    }

    std::size_t Cluster::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM cluster"));
    }

    std::vector<ClusterId> Cluster::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createQuery<ClusterId>(session, params) };

        return utils::execRangeQuery<ClusterId>(query, params.range);
    }

    std::vector<Cluster::pointer> Cluster::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);
        auto query{ createQuery<Wt::Dbo::ptr<Cluster>>(session, params) };

        return utils::execRangeQuery<Cluster::pointer>(query, params.range);
    }

    void Cluster::find(ISession& session, const FindParameters& params, std::function<void(const pointer& cluster)> _func)
    {
        checkReadTransaction(session);
        auto query{ createQuery<Wt::Dbo::ptr<Cluster>>(session, params) };

        return utils::forEachQueryResult(query, _func);
    }

    std::vector<ClusterId> Cluster::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);
        auto query{ getDboSession(session)->query<ClusterId>("SELECT DISTINCT c.id FROM cluster c WHERE NOT EXISTS(SELECT 1 FROM track_cluster t_c WHERE t_c.cluster_id = c.id)") };

        return utils::execRangeQuery<ClusterId>(query, range);
    }

    Cluster::pointer Cluster::find(ISession& session, ClusterId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<Cluster>().where("id = ?").bind(id));
    }

    std::size_t Cluster::computeTrackCount(ISession& session, ClusterId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(t.id) FROM track t INNER JOIN track_cluster t_c ON t_c.track_id = t.id").where("t_c.cluster_id = ?").bind(id));
    }

    std::size_t Cluster::computeReleaseCount(ISession& session, ClusterId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(DISTINCT t.release_id) FROM track t INNER JOIN track_cluster t_c ON t_c.track_id = t.id").where("t_c.cluster_id = ?").bind(id));
    }

    void Cluster::addTrack(const ObjectPtr<Track>& track)
    {
        _tracks.insert(getDboPtr(track));
    }

    std::vector<TrackId> Cluster::getTracks(std::optional<Range> range) const
    {
        assert(session());

        auto query{ session()->query<TrackId>("SELECT t.id FROM track t INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id").where("c.id = ?").bind(getId()) };

        return utils::execRangeQuery<TrackId>(query, range);
    }

    ClusterType::ClusterType(std::string_view name)
        : _name{ name }
    {
        // As we use the name to uniquely identify clusters and cluster types, we must throw
        if (name.size() > maxNameLength)
            throw Exception{ "ClusterType name is too long: " + std::string{ name } + "'" };
    }

    ClusterType::pointer ClusterType::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<ClusterType>{ new ClusterType{ name } });
    }

    void ClusterType::destroy(ISession& session, std::span<const ClusterTypeId> ids)
    {
        utils::destroyObjects<ClusterType>(session, ids);
    }

    std::size_t ClusterType::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM cluster_type"));
    }

    std::vector<ClusterTypeId> ClusterType::findOrphanIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ClusterTypeId>(
                                              "SELECT c_t.id from cluster_type c_t"
                                              " LEFT OUTER JOIN cluster c ON c_t.id = c.cluster_type_id")
                        .where("c.id IS NULL") };

        return utils::execRangeQuery<ClusterTypeId>(query, range);
    }

    std::vector<ClusterTypeId> ClusterType::findUsed(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ClusterTypeId>(
                                              "SELECT DISTINCT c_t.id from cluster_type c_t")
                        .join("cluster c ON c_t.id = c.cluster_type_id") };

        return utils::execRangeQuery<ClusterTypeId>(query, range);
    }

    void ClusterType::find(ISession& session, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<ClusterType>() };
        return utils::forEachQueryResult(query, func);
    }

    ClusterType::pointer ClusterType::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        if (name.size() > maxNameLength)
            throw Exception{ "Requested ClusterType name is too long: " + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(getDboSession(session)->find<ClusterType>().where("name = ?").bind(name));
    }

    ClusterType::pointer ClusterType::find(ISession& session, ClusterTypeId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<ClusterType>().where("id = ?").bind(id));
    }

    std::vector<ClusterTypeId> ClusterType::findIds(ISession& session, std::optional<Range> range)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ClusterTypeId>("SELECT id from cluster_type") };

        return utils::execRangeQuery<ClusterTypeId>(query, range);
    }

    Cluster::pointer ClusterType::getCluster(const std::string& name) const
    {
        assert(self());
        assert(session());

        if (name.size() > Cluster::maxNameLength)
            throw Exception{ "Requested Cluster name is too long: " + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(session()->find<Cluster>().where("name = ?").bind(name).where("cluster_type_id = ?").bind(getId()));
    }

    std::vector<Cluster::pointer> ClusterType::getClusters() const
    {
        assert(self());
        assert(session());

        return utils::fetchQueryResults<Cluster::pointer>(session()->find<Cluster>().where("cluster_type_id = ?").bind(getId()).orderBy("name"));
    }
} // namespace lms::db
