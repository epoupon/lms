/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "lmscore/database/User.hpp"

#include "lmscore/database/Artist.hpp"
#include "lmscore/database/Release.hpp"
#include "lmscore/database/Session.hpp"
#include "lmscore/database/Track.hpp"
#include "lmscore/database/TrackList.hpp"
#include "utils/Logger.hpp"
#include "StringViewTraits.hpp"
#include "Traits.hpp"

namespace Database {


AuthToken::AuthToken(const std::string& value, const Wt::WDateTime& expiry, ObjectPtr<User> user)
: _value {value}
, _expiry {expiry}
, _user {getDboPtr(user)}
{

}

AuthToken::pointer
AuthToken::create(Session& session, const std::string& value, const Wt::WDateTime& expiry, ObjectPtr<User> user)
{
	session.checkUniqueLocked();

	AuthToken::pointer res {session.getDboSession().add(std::make_unique<AuthToken>(value, expiry, user))};
	session.getDboSession().flush();

	return res;
}

void
AuthToken::removeExpiredTokens(Session& session, const Wt::WDateTime& now)
{
	session.checkUniqueLocked();

	session.getDboSession().execute
		("DELETE FROM auth_token WHERE expiry < ?").bind(now);
}

AuthToken::pointer
AuthToken::getByValue(Session& session, const std::string& value)
{
	session.checkSharedLocked();

	return session.getDboSession().find<AuthToken>()
		.where("value = ?").bind(value)
		.resultValue();
}

static const std::string queuedListName {"__queued_tracks__"};

User::User(std::string_view loginName)
: _loginName {loginName}
{
}

std::vector<User::pointer>
User::getAll(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().find<User>().resultList()};
	return std::vector<pointer>(res.begin(), res.end());
}

std::vector<UserId>
User::getAllIds(Session& session)
{
	session.checkSharedLocked();

	auto res {session.getDboSession().query<UserId>("SELECT id FROM user").resultList()};
	return std::vector<UserId>(res.begin(), res.end());
}

User::pointer
User::getDemo(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().find<User>().where("type = ?").bind(UserType::DEMO).resultValue();
}

std::size_t
User::getCount(Session& session)
{
	session.checkSharedLocked();

	return session.getDboSession().query<int>("SELECT COUNT(*) FROM user");
}

User::pointer
User::create(Session& session, std::string_view loginName)
{
	session.checkUniqueLocked();

	User::pointer user {session.getDboSession().add(std::make_unique<User>(loginName))};

	TrackList::create(session, queuedListName, TrackList::Type::Internal, false, user);

	session.getDboSession().flush();

	return user;
}

User::pointer
User::getById(Session& session, UserId id)
{
	return session.getDboSession().find<User>().where("id = ?").bind(id).resultValue();
}

User::pointer
User::getByLoginName(Session& session, std::string_view name)
{
	return session.getDboSession().find<User>()
		.where("login_name = ?").bind(name)
		.resultValue();
}

void
User::setSubsonicTranscodeBitrate(Bitrate bitrate)
{
	assert(audioTranscodeAllowedBitrates.find(bitrate) != audioTranscodeAllowedBitrates.cend());
	_subsonicTranscodeBitrate = bitrate;
}

void
User::clearAuthTokens()
{
	_authTokens.clear();
}

TrackList::pointer
User::getQueuedTrackList(Session& session) const
{
	assert(self());
	session.checkSharedLocked();

	return TrackList::get(session, queuedListName, TrackList::Type::Internal, self());
}

void
User::starArtist(ObjectPtr<Artist> artist)
{
	if (_starredArtists.count(getDboPtr(artist)) == 0)
		_starredArtists.insert(getDboPtr(artist));
}

void
User::unstarArtist(ObjectPtr<Artist> artist)
{
	if (_starredArtists.count(getDboPtr(artist)) != 0)
		_starredArtists.erase(getDboPtr(artist));
}

bool
User::hasStarredArtist(ObjectPtr<Artist> artist) const
{
	return _starredArtists.count(getDboPtr(artist)) != 0;
}

void
User::starRelease(ObjectPtr<Release> release)
{
	if (_starredReleases.count(getDboPtr(release)) == 0)
		_starredReleases.insert(getDboPtr(release));
}

void
User::unstarRelease(ObjectPtr<Release> release)
{
	if (_starredReleases.count(getDboPtr(release)) != 0)
		_starredReleases.erase(getDboPtr(release));
}

bool
User::hasStarredRelease(ObjectPtr<Release> release) const
{
	return _starredReleases.count(getDboPtr(release)) != 0;
}

void
User::starTrack(ObjectPtr<Track> track)
{
	if (_starredTracks.count(getDboPtr(track)) == 0)
		_starredTracks.insert(getDboPtr(track));
}

void
User::unstarTrack(ObjectPtr<Track> track)
{
	if (_starredTracks.count(getDboPtr(track)) != 0)
		_starredTracks.erase(getDboPtr(track));
}

bool
User::hasStarredTrack(ObjectPtr<Track> track) const
{
	return _starredTracks.count(getDboPtr(track)) != 0;
}

} // namespace Database


