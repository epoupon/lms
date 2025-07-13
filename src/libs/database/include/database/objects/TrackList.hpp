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
#include "database/objects/Filters.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class Artist;
    class Cluster;
    class ClusterType;
    class PlayListFile;
    class Release;
    class Session;
    class Track;
    class TrackListEntry;
    class User;

    class TrackList final : public Object<TrackList, TrackListId>
    {
    public:
        TrackList() = default;

        enum class Visibility
        {
            Private = 0,
            Public = 1,
        };

        // Search utility
        struct FindParameters
        {
            Filters filters;
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords (on either name field OR sort name field)
            std::optional<Range> range;
            std::optional<TrackListType> type;
            UserId user;         // only tracklists owned by this user
            UserId excludedUser; // only tracklists *not* owned by this user
            TrackListSortMethod sortMethod{ TrackListSortMethod::None };
            std::optional<Visibility> visibility;

            FindParameters& setFilters(const Filters& _filters)
            {
                filters = _filters;
                return *this;
            }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setType(TrackListType _type)
            {
                type = _type;
                return *this;
            }
            FindParameters& setUser(UserId _user)
            {
                user = _user;
                return *this;
            }
            FindParameters& setExcludedUser(UserId _user)
            {
                excludedUser = _user;
                return *this;
            }

            FindParameters& setSortMethod(TrackListSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
            FindParameters& setVisibility(std::optional<Visibility> _visibility)
            {
                visibility = _visibility;
                return *this;
            }
        };
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, std::string_view name, TrackListType type, UserId userId);
        static pointer find(Session& session, TrackListId tracklistId);
        static RangeResults<TrackListId> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& params, const std::function<void(const TrackList::pointer&)>& func);

        // Accessors
        std::string_view getName() const { return _name; }
        Visibility getVisibility() const { return _visibility; }
        TrackListType getType() const { return _type; }
        ObjectPtr<User> getUser() const { return _user; }
        UserId getUserId() const { return _user.id(); }
        Wt::WDateTime getLastModifiedDateTime() const { return _lastModifiedDateTime; }
        Wt::WDateTime getCreationDateTime() const { return _creationDateTime; }

        // Modifiers
        void setUser(ObjectPtr<User> user) { _user = getDboPtr(user); }
        void setName(std::string_view name) { _name = name; }
        void setVisibility(Visibility visibility) { _visibility = visibility; }
        void clear() { _entries.clear(); }

        // Get tracks, ordered by position
        bool isEmpty() const;
        std::size_t getCount() const;
        ObjectPtr<TrackListEntry> getEntry(std::size_t pos) const;
        RangeResults<ObjectPtr<TrackListEntry>> getEntries(std::optional<Range> range = {}) const;
        ObjectPtr<TrackListEntry> getEntryByTrackAndDateTime(ObjectPtr<Track> track, const Wt::WDateTime& dateTime) const;

        std::vector<TrackId> getTrackIds() const;
        std::chrono::milliseconds getDuration() const;

        void setLastModifiedDateTime(const Wt::WDateTime& dateTime);

        // Get clusters, order by occurence
        std::vector<ObjectPtr<Cluster>> getClusters() const;
        std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ClusterTypeId>& clusterTypeIds, std::size_t size) const;

        // Ordered from most clusters in common
        std::vector<ObjectPtr<Track>> getSimilarTracks(std::optional<std::size_t> offset = {}, std::optional<std::size_t> size = {}) const;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _visibility, "visibility");
            Wt::Dbo::field(a, _creationDateTime, "creation_date_time");
            Wt::Dbo::field(a, _lastModifiedDateTime, "last_modified_date_time");

            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);                  // optional
            Wt::Dbo::belongsTo(a, _playListFile, "playlist_file", Wt::Dbo::OnDeleteCascade); // optional

            Wt::Dbo::hasMany(a, _entries, Wt::Dbo::ManyToOne, "tracklist");
        }

    private:
        friend class Session;
        TrackList(std::string_view name, TrackListType type);
        static pointer create(Session& session, std::string_view name, TrackListType type);

        std::string _name;
        TrackListType _type{ TrackListType::PlayList };
        Visibility _visibility{ Visibility::Private };
        Wt::WDateTime _creationDateTime;
        Wt::WDateTime _lastModifiedDateTime;

        Wt::Dbo::ptr<User> _user;
        Wt::Dbo::ptr<PlayListFile> _playListFile;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> _entries;
    };

    class TrackListEntry final : public Object<TrackListEntry, TrackListEntryId>
    {
    public:
        TrackListEntry() = default;

        // find utility
        // Search utility
        struct FindParameters
        {
            TrackListId trackList;
            std::optional<Range> range;

            FindParameters& setTrackList(TrackListId _trackList)
            {
                trackList = _trackList;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
        };
        static pointer getById(Session& session, TrackListEntryId id);
        static void find(Session& session, const FindParameters& params, const std::function<void(const TrackListEntry::pointer&)>& func);

        // Accessors
        TrackId getTrackId() const { return _track.id(); }
        ObjectPtr<Track> getTrack() const { return _track; }
        const Wt::WDateTime& getDateTime() const { return _dateTime; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _dateTime, "date_time");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _tracklist, "tracklist", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        TrackListEntry(ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime);
        TrackListEntry(ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist);
        static pointer create(Session& session, ObjectPtr<Track> track, ObjectPtr<TrackList> tracklist, const Wt::WDateTime& dateTime = {});

        Wt::WDateTime _dateTime; // optional date time
        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<TrackList> _tracklist;
    };

} // namespace lms::db
