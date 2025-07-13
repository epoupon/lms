/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ClusterId.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackId.hpp"

namespace lms::db
{
    class Track;
    class ClusterType;
    class Session;

    class Cluster final : public Object<Cluster, ClusterId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        struct FindParameters
        {
            std::optional<Range> range;
            ClusterSortMethod sortMethod{ ClusterSortMethod::None };
            ClusterTypeId clusterType;   // if non empty, clusters that belong to this cluster type
            std::string clusterTypeName; // if non empty, clusters that belong to this cluster type
            TrackId track;               // if set, clusters involved in this track
            ReleaseId release;           // if set, clusters involved in this release

            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setSortMethod(ClusterSortMethod _method)
            {
                sortMethod = _method;
                return *this;
            }
            FindParameters& setClusterType(ClusterTypeId _clusterType)
            {
                clusterType = _clusterType;
                return *this;
            }
            FindParameters& setClusterTypeName(std::string_view _name)
            {
                clusterTypeName = _name;
                return *this;
            }
            FindParameters& setTrack(TrackId _track)
            {
                track = _track;
                return *this;
            }
            FindParameters& setRelease(ReleaseId _release)
            {
                release = _release;
                return *this;
            }
        };

        Cluster() = default;

        // Find utility
        static std::size_t getCount(Session& session);
        static RangeResults<ClusterId> findIds(Session& session, const FindParameters& params);
        static RangeResults<pointer> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& params, std::function<void(const pointer& cluster)> _func);
        static pointer find(Session& session, ClusterId id);
        static RangeResults<ClusterId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);

        // May be very slow
        static std::size_t computeTrackCount(Session& session, ClusterId id);
        static std::size_t computeReleaseCount(Session& session, ClusterId id);

        // Accessors
        std::string_view getName() const { return _name; }
        ObjectPtr<ClusterType> getType() const { return _clusterType; }
        std::size_t getTrackCount() const { return _trackCount; }
        RangeResults<TrackId> getTracks(std::optional<Range> range = std::nullopt) const;
        std::size_t getReleasesCount() const { return _releaseCount; };

        void setReleaseCount(std::size_t releaseCount) { _releaseCount = releaseCount; }
        void setTrackCount(std::size_t trackCount) { _trackCount = trackCount; }
        void addTrack(ObjectPtr<Track> track);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            // cached field since queries are too long
            Wt::Dbo::field(a, _trackCount, "track_count");
            Wt::Dbo::field(a, _releaseCount, "release_count");

            Wt::Dbo::belongsTo(a, _clusterType, "cluster_type", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Cluster(ObjectPtr<ClusterType> type, std::string_view name);
        static pointer create(Session& session, ObjectPtr<ClusterType> type, std::string_view name);

        std::string _name;
        int _trackCount{};
        int _releaseCount{};

        Wt::Dbo::ptr<ClusterType> _clusterType;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };

    class ClusterType final : public Object<ClusterType, ClusterTypeId>
    {
    public:
        ClusterType() = default;

        static constexpr std::size_t maxNameLength{ 512 };

        // Getters
        static std::size_t getCount(Session& session);
        static RangeResults<ClusterTypeId> findIds(Session& session, std::optional<Range> range = std::nullopt);
        static void find(Session& session, const std::function<void(const pointer&)>& func);
        static pointer find(Session& session, std::string_view name);
        static pointer find(Session& session, ClusterTypeId id);
        static RangeResults<ClusterTypeId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt);
        static RangeResults<ClusterTypeId> findUsed(Session& session, std::optional<Range> range = std::nullopt);

        static void remove(Session& session, const std::string& name);

        // Accessors
        std::string_view getName() const { return _name; }
        std::vector<Cluster::pointer> getClusters() const;
        Cluster::pointer getCluster(const std::string& name) const;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToOne, "cluster_type");
        }

    private:
        friend class Session;
        ClusterType(std::string_view name);
        static pointer create(Session& session, std::string_view name);

        std::string _name;
        Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> _clusters;
    };

} // namespace lms::db
