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

#include "services/database/Cluster.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "IdTypeTraits.hpp"
#include "SqlQuery.hpp"
#include "Utils.hpp"

namespace Database
{
    namespace
    {
        Wt::Dbo::Query<Cluster::ClusterFindResult> createQuery(Session& session, const Cluster::FindParameters& params)
        {
            session.checkSharedLocked();

            auto query{ session.getDboSession().query<Cluster::ClusterFindResult>("SELECT DISTINCT c.id,c.name FROM cluster c") };

            if (params.track.isValid() || params.release.isValid())
            {
                query.join("track_cluster t_c ON t_c.cluster_id = c.id");
                query.join("track t ON t.id = t_c.track_id");
            }

            if (params.track.isValid())
                query.where("t.id = ?").bind(params.track);
            if (params.release.isValid())
                query.where("t.release_id = ?").bind(params.release);

            if (params.clusterType.isValid())
                query.where("c.cluster_type_id = ?").bind(params.clusterType);

            return query;
        }
    }

    Cluster::Cluster(ObjectPtr<ClusterType> type, std::string_view name)
        : _name{ std::string {name, 0, _maxNameLength} },
        _clusterType{ getDboPtr(type) }
    {
    }

    Cluster::pointer Cluster::create(Session& session, ObjectPtr<ClusterType> type, std::string_view name)
    {
        return session.getDboSession().add(std::unique_ptr<Cluster> {new Cluster{ type, name }});
    }

    std::size_t Cluster::getCount(Session& session)
    {
        session.checkSharedLocked();

        return session.getDboSession().query<int>("SELECT COUNT(*) FROM cluster");
    }

    RangeResults<Cluster::ClusterFindResult> Cluster::find(Session& session, const FindParameters& params)
    {
        session.checkSharedLocked();
        auto query{ createQuery(session, params) };

        return Utils::execQuery(query, params.range);
    }

    RangeResults<ClusterId> Cluster::findOrphans(Session& session, Range range)
    {
        session.checkSharedLocked();
        auto query{ session.getDboSession().query<ClusterId>("SELECT DISTINCT c.id FROM cluster c WHERE NOT EXISTS(SELECT 1 FROM track_cluster t_c WHERE t_c.cluster_id = c.id)") };

        return Utils::execQuery(query, range);
    }

    Cluster::pointer Cluster::find(Session& session, ClusterId id)
    {
        session.checkSharedLocked();

        return session.getDboSession().find<Cluster>().where("id = ?").bind(id).resultValue();
    }

    void Cluster::addTrack(ObjectPtr<Track> track)
    {
        _tracks.insert(getDboPtr(track));
    }

    RangeResults<TrackId> Cluster::getTracks(Range range) const
    {
        assert(session());

        auto query{ session()->query<TrackId>("SELECT t.id FROM track t INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
                .where("c.id = ?").bind(getId()) };

        return Utils::execQuery(query, range);
    }

    std::size_t Cluster::getReleasesCount() const
    {
        assert(session());

        return session()->query<int>("SELECT COUNT(DISTINCT r.id) FROM release r INNER JOIN track t on t.release_id = r.id INNER JOIN cluster c ON c.id = t_c.cluster_id INNER JOIN track_cluster t_c ON t_c.track_id = t.id")
            .where("c.id = ?").bind(getId());
    }


    ClusterType::ClusterType(std::string_view name)
        : _name{ name }
    {
    }

    ClusterType::pointer ClusterType::create(Session& session, const std::string& name)
    {
        return session.getDboSession().add(std::unique_ptr<ClusterType> {new ClusterType{ name }});
    }

    std::size_t ClusterType::getCount(Session& session)
    {
        session.checkSharedLocked();

        return session.getDboSession().query<int>("SELECT COUNT(*) FROM cluster_type");
    }


    RangeResults<ClusterTypeId> ClusterType::findOrphans(Session& session, Range range)
    {
        session.checkSharedLocked();

        auto query{ session.getDboSession().query<ClusterTypeId>(
                "SELECT c_t.id from cluster_type c_t"
                " LEFT OUTER JOIN cluster c ON c_t.id = c.cluster_type_id")
            .where("c.id IS NULL") };

        return Utils::execQuery(query, range);
    }

    RangeResults<ClusterTypeId> ClusterType::findUsed(Session& session, Range range)
    {
        session.checkSharedLocked();

        auto query{ session.getDboSession().query<ClusterTypeId>(
                "SELECT DISTINCT c_t.id from cluster_type c_t")
            .join("cluster c ON c_t.id = c.cluster_type_id") };

        return Utils::execQuery(query, range);
    }

    ClusterType::pointer ClusterType::find(Session& session, std::string_view name)
    {
        session.checkSharedLocked();

        return session.getDboSession().find<ClusterType>().where("name = ?").bind(std::string{ name }).resultValue();
    }

    ClusterType::pointer ClusterType::find(Session& session, ClusterTypeId id)
    {
        session.checkSharedLocked();

        return session.getDboSession().find<ClusterType>().where("id = ?").bind(id).resultValue();
    }

    RangeResults<ClusterTypeId> ClusterType::find(Session& session, Range range)
    {
        session.checkSharedLocked();

        auto query{ session.getDboSession().query<ClusterTypeId>("SELECT id from cluster_type") };

        return Utils::execQuery(query, range);
    }

    Cluster::pointer ClusterType::getCluster(const std::string& name) const
    {
        assert(self());
        assert(session());

        return session()->find<Cluster>()
            .where("name = ?").bind(name)
            .where("cluster_type_id = ?").bind(getId()).resultValue();
    }

    std::vector<Cluster::pointer> ClusterType::getClusters() const
    {
        assert(self());
        assert(session());

        auto res = session()->find<Cluster>()
            .where("cluster_type_id = ?").bind(getId())
            .orderBy("name")
            .resultList();

        return std::vector<Cluster::pointer>(res.begin(), res.end());
    }
} // namespace Database
