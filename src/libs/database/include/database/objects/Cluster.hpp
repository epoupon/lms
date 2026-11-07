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
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ClusterId.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"

namespace lms::db
{
    class ISession;
    class Track;
    class ClusterType;

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
            ArtistId artist;             // if set, clusters involved in the tracks of this artist
            TrackListId trackList;       // if set, clusters involved in the tracks of this track list

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
            FindParameters& setArtist(ArtistId _artist)
            {
                artist = _artist;
                return *this;
            }
            FindParameters& setTrackList(TrackListId _trackList)
            {
                trackList = _trackList;
                return *this;
            }
        };

        Cluster() = default;

        static pointer create(ISession& session, const ObjectPtr<ClusterType>& type, std::string_view name);
        static void destroy(ISession& session, std::span<const ClusterId> ids);

        // Find utility
        static std::size_t getCount(ISession& session);
        static std::vector<ClusterId> findIds(ISession& session, const FindParameters& params);
        static std::vector<pointer> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, std::function<void(const pointer& cluster)> _func);
        static pointer find(ISession& session, ClusterId id);
        static std::vector<ClusterId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        // May be very slow
        static std::size_t computeTrackCount(ISession& session, ClusterId id);
        static std::size_t computeReleaseCount(ISession& session, ClusterId id);

        // Accessors
        std::string_view getName() const { return _name; }
        ObjectPtr<ClusterType> getType() const { return _clusterType; }
        std::vector<TrackId> getTracks(std::optional<Range> range = std::nullopt) const;
        void addTrack(const ObjectPtr<Track>& track);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");

            Wt::Dbo::belongsTo(a, _clusterType, "cluster_type", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        Cluster(const ObjectPtr<ClusterType>& type, std::string_view name);

        std::string _name;

        Wt::Dbo::ptr<ClusterType> _clusterType;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };

    class ClusterType final : public Object<ClusterType, ClusterTypeId>
    {
    public:
        ClusterType() = default;

        static pointer create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const ClusterTypeId> ids);

        static constexpr std::size_t maxNameLength{ 512 };

        // Getters
        static std::size_t getCount(ISession& session);
        static std::vector<ClusterTypeId> findIds(ISession& session, std::optional<Range> range = std::nullopt);
        static void find(ISession& session, const std::function<void(const pointer&)>& func);
        static pointer find(ISession& session, std::string_view name);
        static pointer find(ISession& session, ClusterTypeId id);
        static std::vector<ClusterTypeId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);
        static std::vector<ClusterTypeId> findUsed(ISession& session, std::optional<Range> range = std::nullopt);

        static void remove(ISession& session, const std::string& name);

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
        ClusterType(std::string_view name);

        std::string _name;
        Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> _clusters;
    };

} // namespace lms::db
