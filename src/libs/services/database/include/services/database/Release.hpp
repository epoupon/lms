/*
 * Copyright (C) 2015 Emeric Poupon
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
#include <vector>

#include <Wt/WDateTime.h>
#include <Wt/Dbo/Dbo.h>

#include "services/database/ArtistId.hpp"
#include "services/database/ClusterId.hpp"
#include "services/database/Object.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"
#include "utils/EnumSet.hpp"
#include "utils/UUID.hpp"

namespace Database
{

    class Artist;
    class Cluster;
    class ClusterType;
    class Release;
    class Session;
    class Track;
    class User;

    class Release final : public Object<Release, ReleaseId>
    {
    public:
        struct FindParameters
        {
            std::vector<ClusterId>              clusters; // if non empty, releases that belong to these clusters
            std::vector<std::string_view>       keywords; // if non empty, name must match all of these keywords
            ReleaseSortMethod                   sortMethod{ ReleaseSortMethod::None };
            Range                               range;
            Wt::WDateTime                       writtenAfter;
            std::optional<DateRange>            dateRange;
            UserId                              starringUser;				// only releases starred by this user
            std::optional<Scrobbler>            scrobbler;					//    and for this scrobbler
            ArtistId                            artist;						// only releases that involved this user
            EnumSet<TrackArtistLinkType>        trackArtistLinkTypes; 			//    and for these link types
            EnumSet<TrackArtistLinkType>        excludedTrackArtistLinkTypes; 	//    but not for these link types
            std::optional<ReleaseTypePrimary>   primaryType;				// if, set, matching this primary type
            EnumSet<ReleaseTypeSecondary>       secondaryTypes;				// Matching all this (if any)

            FindParameters& setClusters(const std::vector<ClusterId>& _clusters) { clusters = _clusters; return *this; }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords) { keywords = _keywords; return *this; }
            FindParameters& setSortMethod(ReleaseSortMethod _sortMethod) { sortMethod = _sortMethod; return *this; }
            FindParameters& setRange(Range _range) { range = _range; return *this; }
            FindParameters& setWrittenAfter(const Wt::WDateTime& _after) { writtenAfter = _after; return *this; }
            FindParameters& setDateRange(const std::optional<DateRange>& _dateRange) { dateRange = _dateRange; return *this; }
            FindParameters& setStarringUser(UserId _user, Scrobbler _scrobbler) { starringUser = _user; scrobbler = _scrobbler; return *this; }
            FindParameters& setArtist(ArtistId _artist, EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {}, EnumSet<TrackArtistLinkType> _excludedTrackArtistLinkTypes = {})
            {
                artist = _artist;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                excludedTrackArtistLinkTypes = _excludedTrackArtistLinkTypes;
                return *this;
            }
        };

        Release() = default;

        // Accessors
        static std::size_t              getCount(Session& session);
        static bool                     exists(Session& session, ReleaseId id);
        static pointer                  find(Session& session, const UUID& MBID);
        static std::vector<pointer>     find(Session& session, const std::string& name);
        static pointer                  find(Session& session, ReleaseId id);
        static RangeResults<ReleaseId>  find(Session& session, const FindParameters& parameters);
        static RangeResults<ReleaseId>  findOrphans(Session& session, Range range); // no track related
        static RangeResults<ReleaseId>  findOrderedByArtist(Session& session, Range range);

        // Get the cluster of the tracks that belong to this release
        // Each clusters are grouped by cluster type, sorted by the number of occurence (max to min)
        // size is the max number of cluster per cluster type
        std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ObjectPtr<ClusterType>>& clusterTypes, std::size_t size) const;

        // Utility functions (if all tracks have the same values, which is legit to not be the case)
        Wt::WDate                   getReleaseDate() const;
        Wt::WDate                   getOriginalReleaseDate() const;
        std::optional<std::string>  getCopyright() const;
        std::optional<std::string>  getCopyrightURL() const;

        // Accessors
        const std::string&                  getName() const { return _name; }
        std::optional<UUID>                 getMBID() const { return UUID::fromString(_MBID); }
        std::optional<std::size_t>          getTotalDisc() const { return _totalDisc; }
        std::size_t                         getDiscCount() const; // may not be total disc (if incomplete for example)
        std::vector<DiscInfo>               getDiscs() const;
        std::chrono::milliseconds           getDuration() const;
        Wt::WDateTime                       getLastWritten() const;
        std::optional<ReleaseTypePrimary>   getPrimaryType() const { return _primaryType; }
        EnumSet<ReleaseTypeSecondary>       getSecondaryTypes() const { return _secondaryTypes; }
        std::string_view                    getArtistDisplayName() const { return _artistDisplayName; }
        std::size_t                         getTracksCount() const;

        // Setters
        void setName(std::string_view name) { _name = name; }
        void setMBID(const std::optional<UUID>& mbid) { _MBID = mbid ? mbid->getAsString() : ""; }
        void setTotalDisc(std::optional<int> totalDisc) { _totalDisc = totalDisc; }
        void setPrimaryType(std::optional<ReleaseTypePrimary> type) { _primaryType = type; }
        void setSecondaryTypes(EnumSet<ReleaseTypeSecondary> types) { _secondaryTypes = types; }
        void setArtistDisplayName(std::string_view name) { _artistDisplayName = name; }

        // Get the artists of this release
        std::vector<ObjectPtr<Artist>>  getArtists(TrackArtistLinkType type = TrackArtistLinkType::Artist) const;
        std::vector<ObjectPtr<Artist>>  getReleaseArtists() const { return getArtists(TrackArtistLinkType::ReleaseArtist); }
        bool                            hasVariousArtists() const;
        std::vector<pointer>            getSimilarReleases(std::optional<std::size_t> offset = {}, std::optional<std::size_t> count = {}) const;


        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _MBID, "mbid");
            Wt::Dbo::field(a, _totalDisc, "total_disc");
            Wt::Dbo::field(a, _primaryType, "primary_type");
            Wt::Dbo::field(a, _secondaryTypes, "secondary_types");
            Wt::Dbo::field(a, _artistDisplayName, "artist_display_name");
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
        }

    private:
        friend class Session;
        Release(const std::string& name, const std::optional<UUID>& MBID = {});
        static pointer create(Session& session, const std::string& name, const std::optional<UUID>& MBID = {});

        Wt::WDate getReleaseDate(bool original) const;

        static constexpr std::size_t _maxNameLength{ 128 };

        std::string                         _name;
        std::string                         _MBID;
        std::optional<int>                  _totalDisc{};
        std::optional<ReleaseTypePrimary>   _primaryType;
        EnumSet<ReleaseTypeSecondary>       _secondaryTypes;
        std::string                         _artistDisplayName;

        Wt::Dbo::collection<Wt::Dbo::ptr<Track>>    _tracks; // Tracks in the release
    };

} // namespace Database


