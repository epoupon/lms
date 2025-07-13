/*
 * Copyright (C) 2024 Emeric Poupon
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
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"

LMS_DECLARE_IDTYPE(PlayQueueId)

namespace lms::db
{
    class Session;
    class Track;
    class User;

    // Usage only for subsonic API
    class PlayQueue final : public Object<PlayQueue, PlayQueueId>
    {
    public:
        PlayQueue() = default;

        static std::size_t getCount(Session& session);
        static pointer find(Session& session, PlayQueueId playQueueId);
        static pointer find(Session& session, UserId userId, std::string_view name);

        // Accessors
        std::string_view getName() const { return _name; }
        std::size_t getCurrentIndex() const { return _currentIndex; }
        std::chrono::milliseconds getCurrentPositionInTrack() const { return _currentPositionInTrack; }
        ObjectPtr<User> getUser() const { return _user; }
        ObjectPtr<Track> getTrack(std::size_t index) const;
        ObjectPtr<Track> getTrackAtCurrentIndex() const;
        // Get tracks, ordered by position
        std::vector<TrackId> getTrackIds() const;
        void visitTracks(const std::function<void(const ObjectPtr<Track>& track)>& visitor) const;
        const Wt::WDateTime getLastModifiedDateTime() const { return _lastModifiedDateTime; }

        // Modifiers
        void setCurrentIndex(std::size_t index) { _currentIndex = index; }
        void setCurrentPositionInTrack(std::chrono::milliseconds position) { _currentPositionInTrack = position; }
        void clear();
        void addTrack(const ObjectPtr<Track>& track);
        void setLastModifiedDateTime(const Wt::WDateTime& lastModified) { _lastModifiedDateTime = lastModified; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _currentIndex, "current_index");
            Wt::Dbo::field(a, _currentPositionInTrack, "current_position_in_track");
            Wt::Dbo::field(a, _lastModifiedDateTime, "last_modified_date_time");

            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "playqueue_track", "", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        PlayQueue(const ObjectPtr<User>& user, std::string_view name);
        static pointer create(Session& session, const ObjectPtr<User>& user, std::string_view name);

        std::string _name;
        int _currentIndex{};
        std::chrono::duration<int, std::milli> _currentPositionInTrack{};
        Wt::WDateTime _lastModifiedDateTime;

        Wt::Dbo::ptr<User> _user;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };
} // namespace lms::db
