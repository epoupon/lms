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

#include "TrackList.hpp"

namespace Database {

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
: _maxAudioTranscodeBitrate{static_cast<int>(*audioTranscodeAllowedBitrates.rbegin())}
{

}

std::vector<User::pointer>
User::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.find<User>();
	return std::vector<pointer>(res.begin(), res.end());
}

User::pointer
User::getDemo(Wt::Dbo::Session& session)
{
	pointer res = session.find<User>().where("type = ?").bind(Type::DEMO);
	return res;
}

User::pointer
User::create(Wt::Dbo::Session& session)
{
	return session.add(std::make_unique<User>());
}

User::pointer
User::getById(Wt::Dbo::Session& session, IdType id)
{
	return session.find<User>().where("id = ?").bind( id );
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
User::getPlayedTrackList() const
{
	static const std::string listName = "__played_tracks__";

	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	auto res = TrackList::get(*session(), listName, TrackList::Type::Internal, self());
	if (!res)
		res = TrackList::create(*session(), listName, TrackList::Type::Internal, false, self());

	return res;
}

Wt::Dbo::ptr<TrackList>
User::getQueuedTrackList() const
{
	static const std::string listName = "__queued_tracks__";

	assert(self());
	assert(IdIsValid(self()->id()));
	assert(session());

	auto res = TrackList::get(*session(), listName, TrackList::Type::Internal, self());
	if (!res)
		res = TrackList::create(*session(), listName, TrackList::Type::Internal, false, self());

	return res;
}

} // namespace Database


