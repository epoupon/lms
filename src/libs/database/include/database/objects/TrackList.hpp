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

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "database/objects/FilterExpr.hpp"
#include "database/objects/PlayListFileId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class TrackListEntry;

    class TrackList : public detail::Object<TrackList, TrackListId>
    {
    public:
        static TrackList create(ISession& session, std::string_view name, TrackListType type);
        static void destroy(ISession& session, std::span<const TrackListId> ids);

        enum class Visibility
        {
            Private = 0,
            Public = 1,
        };

        // Search utility
        struct FindParameters
        {
            std::optional<FilterExpr> filter;       // if set, tracklists must have a track matching this expression
            UserId filterUser;                      // required if filter has per-user fields
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords (on either name field OR sort name field)
            std::optional<Range> range;
            std::optional<TrackListType> type;
            UserId user;         // only tracklists owned by this user
            UserId excludedUser; // only tracklists *not* owned by this user
            TrackListSortMethod sortMethod{ TrackListSortMethod::None };
            std::optional<Visibility> visibility;

            FindParameters& setFilter(std::optional<FilterExpr> _filter, UserId _user = {})
            {
                filter = std::move(_filter);
                filterUser = _user;
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
        static std::size_t getCount(ISession& session);
        static std::optional<TrackList> find(ISession& session, std::string_view name, TrackListType type, UserId userId);
        static std::optional<TrackList> find(ISession& session, TrackListId tracklistId);
        static std::optional<TrackList> find(ISession& session, PlayListFileId playListFileId); // the track list made from this playlist file
        static std::vector<TrackListId> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const TrackList&)>& func);

        // Computed from the entries
        static bool isEmpty(ISession& session, TrackListId id);
        static std::size_t getTrackCount(ISession& session, TrackListId id);
        static std::chrono::milliseconds getDuration(ISession& session, TrackListId id);

        // Updates
        static void update(ISession& session, const TrackList& trackList);
        static void clear(ISession& session, TrackListId id);

        // Accessors
        std::string_view getName() const { return _name; }
        Visibility getVisibility() const { return _visibility; }
        TrackListType getType() const { return _type; }
        UserId getUserId() const { return _userId; }
        PlayListFileId getPlayListFileId() const { return _playListFileId; }
        Wt::WDateTime getLastModifiedDateTime() const { return _lastModifiedDateTime; }
        Wt::WDateTime getCreationDateTime() const { return _creationDateTime; }

        // Modifiers
        void setUser(UserId user) { _userId = user; }
        void setPlayListFile(PlayListFileId playListFile) { _playListFileId = playListFile; }
        void setName(std::string_view name) { _name = name; }
        void setVisibility(Visibility visibility) { _visibility = visibility; }
        void setLastModifiedDateTime(const Wt::WDateTime& dateTime);

    protected:
        TrackList() = default;
        TrackList(std::string_view name, TrackListType type);

        std::string _name;
        TrackListType _type{ TrackListType::PlayList };
        Visibility _visibility{ Visibility::Private };
        Wt::WDateTime _creationDateTime;
        Wt::WDateTime _lastModifiedDateTime;

        UserId _userId;
        PlayListFileId _playListFileId;
    };

    class TrackListEntry : public detail::Object<TrackListEntry, TrackListEntryId>
    {
    public:
        static TrackListEntry create(ISession& session, TrackId track, TrackListId tracklist, const Wt::WDateTime& dateTime = {});
        static void destroy(ISession& session, std::span<const TrackListEntryId> ids);

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
        static std::optional<TrackListEntry> getById(ISession& session, TrackListEntryId id);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const TrackListEntry&)>& func);

        // Accessors
        TrackId getTrackId() const { return _trackId; }
        const Wt::WDateTime& getDateTime() const { return _dateTime; }

    protected:
        TrackListEntry() = default;
        explicit TrackListEntry(const Wt::WDateTime& dateTime);

        Wt::WDateTime _dateTime; // optional date time
        TrackId _trackId;
    };

} // namespace lms::db
