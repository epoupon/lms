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

#include "database/objects/TrackList.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class PlayListFile;
    class Track;
    class TrackList;
    class TrackListEntry;
    class User;

    class TrackList final : public db::TrackList, public Wt::Dbo::Dbo<TrackList>
    {
    public:
        TrackList() = default;
        using db::TrackList::TrackList;

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

        Wt::Dbo::ptr<User> _user;
        Wt::Dbo::ptr<PlayListFile> _playListFile;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> _entries;
    };

    class TrackListEntry final : public db::TrackListEntry, public Wt::Dbo::Dbo<TrackListEntry>
    {
    public:
        TrackListEntry() = default;
        using db::TrackListEntry::TrackListEntry;

        TrackListEntry(const ObjectPtr<db::Track>& track, const ObjectPtr<db::TrackList>& tracklist, const Wt::WDateTime& dateTime);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _dateTime, "date_time");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _tracklist, "tracklist", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<TrackList> _tracklist;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackList>
    {
        using type = dbo::TrackList;
    };

    template<>
    struct DboOf<TrackListEntry>
    {
        using type = dbo::TrackListEntry;
    };
} // namespace lms::db
