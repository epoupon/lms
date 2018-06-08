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

#include "TrackStats.hpp"

#include "Artist.hpp"
#include "Release.hpp"
#include "Track.hpp"
#include "User.hpp"

namespace Database {

TrackStats::TrackStats(Wt::Dbo::ptr<Track> track, Wt::Dbo::ptr<User> user)
: _track(track),
  _user(user)
{}

std::vector<Artist::pointer>
TrackStats::getMostPlayedArtists(Wt::Dbo::Session& session, User::pointer user, int limit)
{
	Wt::Dbo::collection<Artist::pointer> res = session.query<Artist::pointer>
		("SELECT a FROM artist a INNER JOIN track t ON a.id = t.artist_id INNER JOIN track_stats t_s ON t.id = t_s.track_id")
		.where("t_s.user_id = ?").bind(user.id())
		.groupBy("a.id")
		.orderBy("SUM(t_s.play_count) DESC")
		.limit(limit);

	return std::vector<Artist::pointer>(res.begin(), res.end());
}

std::vector<Artist::pointer>
TrackStats::getLastPlayedArtists(Wt::Dbo::Session& session, User::pointer user, int limit)
{
	Wt::Dbo::collection<Artist::pointer> res = session.query<Artist::pointer>
		("SELECT a FROM artist a INNER JOIN track t ON a.id = t.artist_id INNER JOIN track_stats t_s ON t.id = t_s.track_id")
		.where("t_s.user_id = ?").bind(user.id())
		.groupBy("a.id")
		.orderBy("t_s.last_played DESC")
		.limit(limit);

	return std::vector<Artist::pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
TrackStats::getMostPlayedReleases(Wt::Dbo::Session& session, User::pointer user, int limit)
{
	Wt::Dbo::collection<Release::pointer> res = session.query<Release::pointer>
		("SELECT r FROM release r INNER JOIN track t ON r.id = t.release_id INNER JOIN track_stats t_s ON t.id = t_s.track_id")
		.where("t_s.user_id = ?").bind(user.id())
		.groupBy("r.id")
		.orderBy("SUM(t_s.play_count) DESC")
		.limit(limit);

	return std::vector<Release::pointer>(res.begin(), res.end());
}

std::vector<Release::pointer>
TrackStats::getLastPlayedReleases(Wt::Dbo::Session& session, User::pointer user, int limit)
{
	Wt::Dbo::collection<Release::pointer> res = session.query<Release::pointer>
		("SELECT r FROM release r INNER JOIN track t ON r.id = t.release_id INNER JOIN track_stats t_s ON t.id = t_s.track_id")
		.where("t_s.user_id = ?").bind(user.id())
		.groupBy("r.id")
		.orderBy("t_s.last_played DESC")
		.limit(limit);

	return std::vector<Release::pointer>(res.begin(), res.end());
}

TrackStats::pointer
TrackStats::get(Wt::Dbo::Session& session, Track::pointer track, User::pointer user)
{
	Wt::Dbo::Transaction transaction(session);

	TrackStats::pointer res = session.find<TrackStats>()
		.where("track_id = ?").bind(track.id())
		.where("user_id = ?").bind(user.id());
	if (!res)
		res = session.add(std::make_unique<TrackStats>(track, user));

	return res;
}

} // namespace Database

