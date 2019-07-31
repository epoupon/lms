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

#include "User.hpp"

#include "utils/Logger.hpp"
#include "Artist.hpp"
#include "Release.hpp"
#include "Session.hpp"
#include "Track.hpp"
#include "TrackList.hpp"

namespace Database {


AuthToken::AuthToken(const std::string& value, const Wt::WDateTime& expiry, Wt::Dbo::ptr<User> user)
: _value {value}
, _expiry {expiry}
, _user {user}
{

}

AuthToken::pointer
AuthToken::create(Session& session, const std::string& value, const Wt::WDateTime& expiry, Wt::Dbo::ptr<User> user)
{
	session.checkUniqueLocked();

	auto res {session.getDboSession().add(std::make_unique<AuthToken>(value, expiry, user))};

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
		.where("value = ?").bind(value);
}

static const std::string playedListName {"__played_tracks__"};
static const std::string queuedListName {"__queued_tracks__"};

const std::set<Bitrate>
User::audioTranscodeAllowedBitrates =
{
	64000,
	96000,
	128000,
	192000,
	320000,
};

User::User()
: _maxAudioTranscodeBitrate {static_cast<int>(*audioTranscodeAllowedBitrates.rbegin())}
{

}

User::User(const std::string& loginName, const PasswordHash& passwordHash)
: User()
{
	_loginName = loginName;
	_passwordHash = passwordHash.hash;
	_passwordSalt  = passwordHash.salt;
}

std::vector<User::pointer>
User::getAll(Session& session)
{
	session.checkSharedLocked();

	Wt::Dbo::collection<pointer> res = session.getDboSession().find<User>();
	return std::vector<pointer>(res.begin(), res.end());
}

User::pointer
User::getDemo(Session& session)
{
	session.checkSharedLocked();

	pointer res = session.getDboSession().find<User>().where("type = ?").bind(Type::DEMO);
	return res;
}

User::pointer
User::create(Session& session, const std::string& loginName, const PasswordHash& passwordHash)
{
	session.checkUniqueLocked();

	User::pointer user {session.getDboSession().add(std::make_unique<User>(loginName, passwordHash))};

	TrackList::create(session, playedListName, TrackList::Type::Internal, false, user);
	TrackList::create(session, queuedListName, TrackList::Type::Internal, false, user);

	session.getDboSession().flush();

	return user;
}

User::pointer
User::getById(Session& session, IdType id)
{
	return session.getDboSession().find<User>().where("id = ?").bind( id );
}

User::pointer
User::getByLoginName(Session& session, const std::string& name)
{
	return session.getDboSession().find<User>()
		.where("login_name = ?").bind(name);
}

void
User::setAudioTranscodeBitrate(Bitrate bitrate)
{
	_audioTranscodeBitrate = std::min(bitrate, static_cast<Bitrate>(_maxAudioTranscodeBitrate));
}

void
User::setMaxAudioTranscodeBitrate(Bitrate bitrate)
{
	_maxAudioTranscodeBitrate = std::min(bitrate, *audioTranscodeAllowedBitrates.rbegin());
	if (_audioTranscodeBitrate > _maxAudioTranscodeBitrate)
		_audioTranscodeBitrate = _maxAudioTranscodeBitrate;
}

void
User::clearAuthTokens()
{
	_authTokens.clear();
}

Bitrate
User::getAudioTranscodeBitrate(void) const
{
	return _audioTranscodeBitrate;
}

std::size_t
User::getMaxAudioTranscodeBitrate(void) const
{
	return _maxAudioTranscodeBitrate;
}

Wt::Dbo::ptr<TrackList>
User::getPlayedTrackList(Session& session) const
{
	assert(self());
	session.checkSharedLocked();

	return TrackList::get(session, playedListName, TrackList::Type::Internal, self());
}

Wt::Dbo::ptr<TrackList>
User::getQueuedTrackList(Session& session) const
{
	assert(self());
	session.checkSharedLocked();

	return TrackList::get(session, queuedListName, TrackList::Type::Internal, self());
}

void
User::starArtist(Wt::Dbo::ptr<Artist> artist)
{
	if (_starredArtists.count(artist) == 0)
		_starredArtists.insert(artist);
}

void
User::unstarArtist(Wt::Dbo::ptr<Artist> artist)
{
	if (_starredArtists.count(artist) != 0)
		_starredArtists.erase(artist);
}

bool
User::hasStarredArtist(Wt::Dbo::ptr<Artist> artist) const
{
	return _starredArtists.count(artist) != 0;
}

std::vector<Wt::Dbo::ptr<Artist>>
User::getStarredArtists() const
{
	return std::vector<Wt::Dbo::ptr<Artist>>(_starredArtists.begin(), _starredArtists.end());
}

void
User::starRelease(Wt::Dbo::ptr<Release> release)
{
	if (_starredReleases.count(release) == 0)
		_starredReleases.insert(release);
}

void
User::unstarRelease(Wt::Dbo::ptr<Release> release)
{
	if (_starredReleases.count(release) != 0)
		_starredReleases.erase(release);
}

bool
User::hasStarredRelease(Wt::Dbo::ptr<Release> release) const
{
	return _starredReleases.count(release) != 0;
}

std::vector<Wt::Dbo::ptr<Release>>
User::getStarredReleases() const
{
	return std::vector<Wt::Dbo::ptr<Release>>(_starredReleases.begin(), _starredReleases.end());
}


void
User::starTrack(Wt::Dbo::ptr<Track> track)
{
	if (_starredTracks.count(track) == 0)
		_starredTracks.insert(track);
}

void
User::unstarTrack(Wt::Dbo::ptr<Track> track)
{
	if (_starredTracks.count(track) != 0)
		_starredTracks.erase(track);
}

bool
User::hasStarredTrack(Wt::Dbo::ptr<Track> track) const
{
	return _starredTracks.count(track) != 0;
}

std::vector<Wt::Dbo::ptr<Track>>
User::getStarredTracks() const
{
	return std::vector<Wt::Dbo::ptr<Track>>(_starredTracks.begin(), _starredTracks.end());
}

} // namespace Database


