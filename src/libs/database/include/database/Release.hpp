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

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"
#include "core/UUID.hpp"
#include "database/ArtistId.hpp"
#include "database/ClusterId.hpp"
#include "database/DirectoryId.hpp"
#include "database/MediaLibraryId.hpp"
#include "database/Object.hpp"
#include "database/ReleaseId.hpp"
#include "database/ReleaseTypeId.hpp"
#include "database/Types.hpp"
#include "database/UserId.hpp"

namespace lms::db
{
    class Artist;
    class Cluster;
    class ClusterType;
    class Release;
    class Session;
    class Track;
    class User;

    class ReleaseType final : public Object<ReleaseType, ReleaseTypeId>
    {
    public:
        ReleaseType() = default;
        static pointer find(Session& session, ReleaseTypeId id);
        static pointer find(Session& session, std::string_view name);

        // Accessors
        std::string_view getName() const { return _name; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _releases, Wt::Dbo::ManyToMany, "release_release_type", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        static constexpr std::size_t _maxNameLength{ 512 };

        friend class Session;
        ReleaseType(std::string_view name);
        static pointer create(Session& session, std::string_view name);

        std::string _name;
        Wt::Dbo::collection<Wt::Dbo::ptr<Release>> _releases; // releases that match this type
    };

    class Release final : public Object<Release, ReleaseId>
    {
    public:
        struct FindParameters
        {
            std::vector<ClusterId> clusters;        // if non empty, releases that belong to these clusters
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            ReleaseSortMethod sortMethod{ ReleaseSortMethod::None };
            std::optional<Range> range;
            Wt::WDateTime writtenAfter;
            std::optional<DateRange> dateRange;
            UserId starringUser;                                             // only releases starred by this user
            std::optional<FeedbackBackend> feedbackBackend;                  //    and for this backend
            ArtistId artist;                                                 // only releases that involved this user
            core::EnumSet<TrackArtistLinkType> trackArtistLinkTypes;         //    and for these link types
            core::EnumSet<TrackArtistLinkType> excludedTrackArtistLinkTypes; //    but not for these link types
            std::string releaseType;                                         // If set, albums that has this release type
            MediaLibraryId mediaLibrary;                                     // If set, releases that has at least a track in this library
            DirectoryId directory;                                           // if set, tracks in this directory

            FindParameters& setClusters(std::span<const ClusterId> _clusters)
            {
                clusters.assign(std::cbegin(_clusters), std::cend(_clusters));
                return *this;
            }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            FindParameters& setSortMethod(ReleaseSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setWrittenAfter(const Wt::WDateTime& _after)
            {
                writtenAfter = _after;
                return *this;
            }
            FindParameters& setDateRange(const std::optional<DateRange>& _dateRange)
            {
                dateRange = _dateRange;
                return *this;
            }
            FindParameters& setStarringUser(UserId _user, FeedbackBackend _feedbackBackend)
            {
                starringUser = _user;
                feedbackBackend = _feedbackBackend;
                return *this;
            }
            FindParameters& setArtist(ArtistId _artist, core::EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {}, core::EnumSet<TrackArtistLinkType> _excludedTrackArtistLinkTypes = {})
            {
                artist = _artist;
                trackArtistLinkTypes = _trackArtistLinkTypes;
                excludedTrackArtistLinkTypes = _excludedTrackArtistLinkTypes;
                return *this;
            }
            FindParameters& setReleaseType(std::string_view _releaseType)
            {
                releaseType = _releaseType;
                return *this;
            }
            FindParameters& setMediaLibrary(MediaLibraryId _mediaLibrary)
            {
                mediaLibrary = _mediaLibrary;
                return *this;
            }
            FindParameters& setDirectory(DirectoryId _directory)
            {
                directory = _directory;
                return *this;
            }
        };

        Release() = default;

        // Accessors
        static std::size_t getCount(Session& session);
        static bool exists(Session& session, ReleaseId id);
        static pointer find(Session& session, const core::UUID& MBID);
        static std::vector<pointer> find(Session& session, const std::string& name, const std::filesystem::path& releaseDirectory);
        static pointer find(Session& session, ReleaseId id);
        static void find(Session& session, ReleaseId& lastRetrievedRelease, std::size_t count, const std::function<void(const Release::pointer&)>& func, MediaLibraryId library = {});
        static RangeResults<pointer> find(Session& session, const FindParameters& parameters);
        static void find(Session& session, const FindParameters& parameters, const std::function<void(const pointer&)>& func);
        static RangeResults<ReleaseId> findIds(Session& session, const FindParameters& parameters);
        static std::size_t getCount(Session& session, const FindParameters& parameters);
        static RangeResults<ReleaseId> findOrphanIds(Session& session, std::optional<Range> range = std::nullopt); // not track related

        // Get the cluster of the tracks that belong to this release
        // Each clusters are grouped by cluster type, sorted by the number of occurence (max to min)
        // size is the max number of cluster per cluster type
        std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ClusterTypeId>& clusterTypeIds, std::size_t size) const;

        // Utility functions (if all tracks have the same values, which is legit to not be the case)
        Wt::WDate getDate() const;
        std::optional<int> getYear() const;
        Wt::WDate getOriginalDate() const;
        std::optional<int> getOriginalYear() const;
        std::optional<std::string> getCopyright() const;
        std::optional<std::string> getCopyrightURL() const;
        std::size_t getMeanBitrate() const;

        // Accessors
        std::string_view getName() const { return _name; }
        std::string_view getSortName() const { return _sortName; }
        std::optional<core::UUID> getMBID() const { return core::UUID::fromString(_MBID); }
        std::optional<core::UUID> getGroupMBID() const { return core::UUID::fromString(_groupMBID); }
        std::optional<std::size_t> getTotalDisc() const { return _totalDisc; }
        std::size_t getDiscCount() const; // may not be total disc (if incomplete for example)
        std::vector<DiscInfo> getDiscs() const;
        std::chrono::milliseconds getDuration() const;
        Wt::WDateTime getLastWritten() const;
        std::string_view getArtistDisplayName() const { return _artistDisplayName; }
        std::size_t getTrackCount() const;
        std::vector<ObjectPtr<ReleaseType>> getReleaseTypes() const;
        std::vector<std::string> getReleaseTypeNames() const;

        // Setters
        void setName(std::string_view name) { _name = name; }
        void setSortName(std::string_view sortName) { _sortName = sortName; }
        void setMBID(const std::optional<core::UUID>& mbid) { _MBID = mbid ? mbid->getAsString() : ""; }
        void setGroupMBID(const std::optional<core::UUID>& mbid) { _groupMBID = mbid ? mbid->getAsString() : ""; }
        void setTotalDisc(std::optional<int> totalDisc) { _totalDisc = totalDisc; }
        void setArtistDisplayName(std::string_view name) { _artistDisplayName = name; }
        void clearReleaseTypes();
        void addReleaseType(ObjectPtr<ReleaseType> releaseType);

        // Get the artists of this release
        std::vector<ObjectPtr<Artist>> getArtists(TrackArtistLinkType type = TrackArtistLinkType::Artist) const;
        std::vector<ObjectPtr<Artist>> getReleaseArtists() const { return getArtists(TrackArtistLinkType::ReleaseArtist); }
        bool hasVariousArtists() const;
        std::vector<pointer> getSimilarReleases(std::optional<std::size_t> offset = {}, std::optional<std::size_t> count = {}) const;
        bool hasDiscSubtitle() const;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _sortName, "sort_name");
            Wt::Dbo::field(a, _MBID, "mbid");
            Wt::Dbo::field(a, _groupMBID, "group_mbid");
            Wt::Dbo::field(a, _totalDisc, "total_disc");
            Wt::Dbo::field(a, _artistDisplayName, "artist_display_name");
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
            Wt::Dbo::hasMany(a, _releaseTypes, Wt::Dbo::ManyToMany, "release_release_type", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Release(const std::string& name, const std::optional<core::UUID>& MBID = {});
        static pointer create(Session& session, const std::string& name, const std::optional<core::UUID>& MBID = {});

        Wt::WDate getDate(bool original) const;
        std::optional<int> getYear(bool original) const;

        static constexpr std::size_t _maxNameLength{ 512 };

        std::string _name;
        std::string _sortName;
        std::string _MBID;
        std::string _groupMBID;
        std::optional<int> _totalDisc{};
        std::string _artistDisplayName;

        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;             // Tracks in the release
        Wt::Dbo::collection<Wt::Dbo::ptr<ReleaseType>> _releaseTypes; // Release types
    };

} // namespace lms::db
