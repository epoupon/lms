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

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"

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
        Wt::Dbo::Query<ResultType> createQuery(Session& session, std::string_view itemToSelect, const Cluster::FindParameters& params)
        {
            session.checkReadTransaction();

            auto query{ session.getDboSession()->query<ResultType>("SELECT " + std::string{ itemToSelect } + " FROM cluster c") };
            query.groupBy("c.id");

            if (params.track.isValid() || params.release.isValid())
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

            assert(!params.clusterType.isValid() || params.clusterTypeName.empty());
            if (params.clusterType.isValid())
                query.where("+c.cluster_type_id = ?").bind(params.clusterType); // Exclude this since the query planner does not do a good job when db is not analyzed
            else if (!params.clusterTypeName.empty())
                query.where("c_t.name = ?").bind(params.clusterTypeName);

            switch (params.sortMethod)
            {
            case ClusterSortMethod::None:
                break;
            case ClusterSortMethod::Name:
                query.orderBy("c.name COLLATE NOCASE");
                break;
            }

            query.groupBy("c.id");

            return query;
        }

        template<typename ResultType>
        Wt::Dbo::Query<ResultType> createQuery(Session& session, const Cluster::FindParameters& params)
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

    Cluster::Cluster(ObjectPtr<ClusterType> type, std::string_view name)
        : _name{ name }
        , _clusterType{ getDboPtr(type) }
    {
        // As we use the name to uniquely identify clusters and cluster types, we must throw (and not truncate)
        if (name.size() > maxNameLength)
            throw Exception{ "Cluster name is too long: " + std::string{ name } + "'" };
    }

    Cluster::pointer Cluster::create(Session& session, ObjectPtr<ClusterType> type, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<Cluster>{ new Cluster{ type, name } });
    }

    std::size_t Cluster::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM cluster"));
    }

    RangeResults<ClusterId> Cluster::findIds(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<ClusterId>(session, params) };

        return utils::execRangeQuery<ClusterId>(query, params.range);
    }

    RangeResults<Cluster::pointer> Cluster::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<Cluster>>(session, params) };

        return utils::execRangeQuery<Cluster::pointer>(query, params.range);
    }

    void Cluster::find(Session& session, const FindParameters& params, std::function<void(const pointer& cluster)> _func)
    {
        session.checkReadTransaction();
        auto query{ createQuery<Wt::Dbo::ptr<Cluster>>(session, params) };

        return utils::forEachQueryResult(query, _func);
    }

    RangeResults<ClusterId> Cluster::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();
        auto query{ session.getDboSession()->query<ClusterId>("SELECT DISTINCT c.id FROM cluster c WHERE NOT EXISTS(SELECT 1 FROM track_cluster t_c WHERE t_c.cluster_id = c.id)") };

        return utils::execRangeQuery<ClusterId>(query, range);
    }

    Cluster::pointer Cluster::find(Session& session, ClusterId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<Cluster>().where("id = ?").bind(id));
    }

    std::size_t Cluster::computeTrackCount(Session& session, ClusterId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(t.id) FROM track t INNER JOIN track_cluster t_c ON t_c.track_id = t.id").where("t_c.cluster_id = ?").bind(id));
    }

    std::size_t Cluster::computeReleaseCount(Session& session, ClusterId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(DISTINCT t.release_id) FROM track t INNER JOIN track_cluster t_c ON t_c.track_id = t.id").where("t_c.cluster_id = ?").bind(id));
    }

    void Cluster::addTrack(ObjectPtr<Track> track)
    {
        _tracks.insert(getDboPtr(track));
    }

    RangeResults<TrackId> Cluster::getTracks(std::optional<Range> range) const
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

    ClusterType::pointer ClusterType::create(Session& session, std::string_view name)
    {
        return session.getDboSession()->add(std::unique_ptr<ClusterType>{ new ClusterType{ name } });
    }

    std::size_t ClusterType::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM cluster_type"));
    }

    RangeResults<ClusterTypeId> ClusterType::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ClusterTypeId>(
                                               "SELECT c_t.id from cluster_type c_t"
                                               " LEFT OUTER JOIN cluster c ON c_t.id = c.cluster_type_id")
                        .where("c.id IS NULL") };

        return utils::execRangeQuery<ClusterTypeId>(query, range);
    }

    RangeResults<ClusterTypeId> ClusterType::findUsed(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ClusterTypeId>(
                                               "SELECT DISTINCT c_t.id from cluster_type c_t")
                        .join("cluster c ON c_t.id = c.cluster_type_id") };

        return utils::execRangeQuery<ClusterTypeId>(query, range);
    }

    void ClusterType::find(Session& session, const std::function<void(const pointer&)>& func)
    {
        auto query{ session.getDboSession()->find<ClusterType>() };
        return utils::forEachQueryResult(query, func);
    }

    ClusterType::pointer ClusterType::find(Session& session, std::string_view name)
    {
        session.checkReadTransaction();

        if (name.size() > maxNameLength)
            throw Exception{ "Requested ClusterType name is too long: " + std::string{ name } + "'" };

        return utils::fetchQuerySingleResult(session.getDboSession()->find<ClusterType>().where("name = ?").bind(name));
    }

    ClusterType::pointer ClusterType::find(Session& session, ClusterTypeId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<ClusterType>().where("id = ?").bind(id));
    }

    RangeResults<ClusterTypeId> ClusterType::findIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ClusterTypeId>("SELECT id from cluster_type") };

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
