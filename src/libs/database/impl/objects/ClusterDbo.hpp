/*
 * Copyright (C) 2026 Emeric Poupon
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

#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/objects/Cluster.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Cluster;
    class ClusterType;
    class Track;

    class Cluster final : public db::Cluster, public Wt::Dbo::Dbo<Cluster>
    {
    public:
        Cluster() = default;
        using db::Cluster::Cluster;

        Cluster(const ObjectPtr<db::ClusterType>& type, std::string_view name);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");

            Wt::Dbo::belongsTo(a, _clusterType, "cluster_type", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<ClusterType> _clusterType;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };

    class ClusterType final : public db::ClusterType, public Wt::Dbo::Dbo<ClusterType>
    {
    public:
        ClusterType() = default;
        using db::ClusterType::ClusterType;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToOne, "cluster_type");
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> _clusters;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Cluster>
    {
        using type = dbo::Cluster;
    };

    template<>
    struct DboOf<ClusterType>
    {
        using type = dbo::ClusterType;
    };
} // namespace lms::db
