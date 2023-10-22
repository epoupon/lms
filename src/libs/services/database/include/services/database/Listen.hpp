/*
 * Copyright (C) 2021 Emeric Poupon
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
#include <Wt/WDateTime.h>

#include "services/database/ArtistId.hpp"
#include "services/database/ClusterId.hpp"
#include "services/database/ListenId.hpp"
#include "services/database/Object.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"

namespace Database
{

    class Session;
    class Track;
    class User;

    class Listen final : public Object<Listen, ListenId>
    {
    public:
        Listen() = default;

        struct FindParameters
        {
            UserId							user;
            std::optional<Scrobbler>		scrobbler;
            std::optional<ScrobblingState>	scrobblingState;
            Range							range;

            FindParameters& setUser(UserId _user) { user = _user; return *this; }
            FindParameters& setScrobbler(Scrobbler _scrobbler) { scrobbler = _scrobbler; return *this; }
            FindParameters& setScrobblingState(ScrobblingState _scrobblingState) { scrobblingState = _scrobblingState; return *this; }
            FindParameters& setRange(Range _range) { range = _range; return *this; }
        };

        // Accessors
        static std::size_t              getCount(Session& session);
        static pointer                  find(Session& session, ListenId id);
        static pointer                  find(Session& session, UserId userId, TrackId trackId, Scrobbler scrobbler, const Wt::WDateTime& dateTime);
        static RangeResults<ListenId>   find(Session& session, const FindParameters& parameters);

        // Stats
        static RangeResults<ArtistId>   getTopArtists(Session& session, UserId userId, Scrobbler scrobbler, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, Range range = {});
        static RangeResults<ReleaseId>  getTopReleases(Session& session, UserId userId, Scrobbler scrobbler, const std::vector<ClusterId>& clusterIds, Range range = {});
        static RangeResults<TrackId>    getTopTracks(Session& session, UserId userId, Scrobbler scrobbler, const std::vector<ClusterId>& clusterIds, Range range = {});

        static RangeResults<ArtistId>   getRecentArtists(Session& session, UserId userId, Scrobbler scrobbler, const std::vector<ClusterId>& clusterIds, std::optional<TrackArtistLinkType> linkType, Range range = {});
        static RangeResults<ReleaseId>  getRecentReleases(Session& session, UserId userId, Scrobbler scrobbler, const std::vector<ClusterId>& clusterIds, Range range = {});
        static RangeResults<TrackId>    getRecentTracks(Session& session, UserId userId, Scrobbler scrobbler, const std::vector<ClusterId>& clusterIds, Range range = {});

        static pointer          getMostRecentListen(Session& session, UserId userId, Scrobbler scrobbler, ReleaseId releaseId);
        static pointer          getMostRecentListen(Session& session, UserId userId, Scrobbler scrobbler, TrackId releaseId);

        ScrobblingState         getScrobblingState() const { return _scrobblingState; }
        ObjectPtr<User>         getUser() const { return _user; }
        ObjectPtr<Track>        getTrack() const { return _track; }
        const Wt::WDateTime& getDateTime() const { return _dateTime; }

        void			setScrobblingState(ScrobblingState state) { _scrobblingState = state; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _dateTime, "date_time");
            Wt::Dbo::field(a, _scrobbler, "scrobbler");
            Wt::Dbo::field(a, _scrobblingState, "scrobbling_state");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Listen(ObjectPtr<User> user, ObjectPtr<Track> track, Scrobbler scrobbler, const Wt::WDateTime& dateTime);
        static pointer create(Session& session, ObjectPtr<User> user, ObjectPtr<Track> track, Scrobbler scrobbler, const Wt::WDateTime& dateTime);

        Wt::WDateTime		_dateTime;
        Scrobbler			_scrobbler;
        ScrobblingState		_scrobblingState{ ScrobblingState::PendingAdd };

        Wt::Dbo::ptr<User>	_user;
        Wt::Dbo::ptr<Track>	_track;
    };

} // namespace Database

