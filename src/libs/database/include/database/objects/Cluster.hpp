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

#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ClusterId.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class ClusterType;

    class Cluster : public detail::Object<Cluster, ClusterId>
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

        static Cluster create(ISession& session, ClusterTypeId type, std::string_view name);
        static void destroy(ISession& session, std::span<const ClusterId> ids);

        // Find utility
        static std::size_t getCount(ISession& session);
        static std::vector<ClusterId> findIds(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const Cluster& cluster)>& func);
        static std::optional<Cluster> find(ISession& session, ClusterId id);
        static std::optional<Cluster> find(ISession& session, ClusterTypeId type, std::string_view name);
        static std::vector<ClusterId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);

        // May be very slow
        static std::size_t computeTrackCount(ISession& session, ClusterId id);
        static std::size_t computeReleaseCount(ISession& session, ClusterId id);

        // Updates
        static void addTrack(ISession& session, ClusterId id, TrackId track);

        // Accessors
        std::string_view getName() const { return _name; }
        ClusterTypeId getTypeId() const { return _typeId; }

    protected:
        Cluster() = default;
        explicit Cluster(std::string_view name);

        std::string _name;

        ClusterTypeId _typeId;
    };

    class ClusterType : public detail::Object<ClusterType, ClusterTypeId>
    {
    public:
        static ClusterType create(ISession& session, std::string_view name);
        static void destroy(ISession& session, std::span<const ClusterTypeId> ids);

        static constexpr std::size_t maxNameLength{ 512 };

        // Getters
        static std::size_t getCount(ISession& session);
        static std::vector<ClusterTypeId> findIds(ISession& session, std::optional<Range> range = std::nullopt);
        static void find(ISession& session, const std::function<void(const ClusterType&)>& func);
        static std::optional<ClusterType> find(ISession& session, std::string_view name);
        static std::optional<ClusterType> find(ISession& session, ClusterTypeId id);
        static std::vector<ClusterTypeId> findOrphanIds(ISession& session, std::optional<Range> range = std::nullopt);
        static std::vector<ClusterTypeId> findUsed(ISession& session, std::optional<Range> range = std::nullopt);

        static void remove(ISession& session, const std::string& name);

        // Accessors
        std::string_view getName() const { return _name; }

    protected:
        ClusterType() = default;
        explicit ClusterType(std::string_view name);

        std::string _name;
    };

} // namespace lms::db
