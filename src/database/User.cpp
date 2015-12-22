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

#include "Types.hpp"

namespace Database {

// must be ordered
const std::vector<std::size_t>
User::audioBitrates =
{
	64000,
	96000,
	128000,
	160000,
	192000,
	224000,
	256000,
	320000,
	512000
};


const std::vector<std::size_t>
User::videoBitrates =
{
	256000,
	512000,
	1024000,
	2048000,
	4096000,
	8192000
};

const std::vector<AudioEncoding>
User::audioEncodings =
{
	AudioEncoding::AUTO,
	AudioEncoding::MP3,
	AudioEncoding::OGA,
	AudioEncoding::WEBMA,
};

const std::vector<VideoEncoding>
User::videoEncodings =
{
	VideoEncoding::AUTO,
};

User::User()
: _maxAudioBitrate(maxAudioBitrate),
 _maxVideoBitrate(maxVideoBitrate),
_isAdmin(false),
_audioBitrate(defaultAudioBitrate),
_audioEncoding(AudioEncoding::AUTO),
_videoBitrate(defaultVideoBitrate),
_videoEncoding(VideoEncoding::AUTO),
_curPlayingTrackPos(0)
{

}

std::vector<User::pointer>
User::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.find<User>();
	return std::vector<pointer>(res.begin(), res.end());
}

User::pointer
User::getById(Wt::Dbo::Session& session, std::string id)
{
	return session.find<User>().where("id = ?").bind( id );
}

std::string
User::getId( pointer user)
{
	std::ostringstream oss; oss << user.id();
	return oss.str();
}

void
User::setAudioBitrate(std::size_t bitrate)
{
	_audioBitrate = std::min(bitrate, std::min(static_cast<std::size_t>(_maxAudioBitrate), audioBitrates.back()));
}

void
User::setVideoBitrate(std::size_t bitrate)
{
	_videoBitrate = std::min(bitrate, std::min(static_cast<std::size_t>(_maxVideoBitrate), videoBitrates.back()));
}

void
User::setMaxAudioBitrate(std::size_t bitrate)
{
	_maxAudioBitrate = std::min(bitrate, static_cast<std::size_t>(_maxAudioBitrate));
}

void
User::setMaxVideoBitrate(std::size_t bitrate)
{
	_maxVideoBitrate = std::min(bitrate, static_cast<std::size_t>(_maxVideoBitrate));
}

std::size_t
User::getAudioBitrate(void) const
{
	if (!isAdmin())
		return std::min(static_cast<std::size_t>(_audioBitrate), std::min(static_cast<std::size_t>(_maxAudioBitrate), audioBitrates.back()));
	else
		return std::min(static_cast<std::size_t>(_audioBitrate), audioBitrates.back());
}

std::size_t
User::getVideoBitrate(void) const
{
	if (!isAdmin())
		return std::min(static_cast<std::size_t>(_videoBitrate), std::min(static_cast<std::size_t>(_maxVideoBitrate), videoBitrates.back()));
	else
		return std::min(static_cast<std::size_t>(_videoBitrate), videoBitrates.back());
}

std::size_t
User::getMaxAudioBitrate(void) const
{
	if (!isAdmin())
		return std::min(static_cast<std::size_t>(_maxAudioBitrate), audioBitrates.back());
	else
		return audioBitrates.back();
}

std::size_t
User::getMaxVideoBitrate(void) const
{
	if (!isAdmin())
		return std::min(static_cast<std::size_t>(_maxVideoBitrate), videoBitrates.back());
	else
		return videoBitrates.back();
}

} // namespace Database


