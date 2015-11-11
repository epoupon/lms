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

#include <boost/foreach.hpp>

#include "Types.hpp"

static std::string pathsToString(const std::vector<boost::filesystem::path>& paths)
{
	std::ostringstream oss;

	bool first = true;
	for (auto& path : paths)
	{
		if (!first)
			oss << " ";

		oss << path.string();
		first = false;
	}

	return oss.str();
}

static std::vector<boost::filesystem::path> stringToPaths(const std::string value)
{
	std::vector<std::string> res;
	std::istringstream iss(value);

	std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter(res));

	return std::vector<boost::filesystem::path>(res.begin(), res.end());
}

namespace Database {

MediaDirectorySettings::MediaDirectorySettings()
: _manualScanRequested(false),
_updatePeriod(Never),
_audioFileExtensions(".mp3 .ogg .oga .aac .m4a .flac .wav .wma .aif .aiff .ape .mpc .shn"),
_videoFileExtensions(".flv .avi .mpg .mpeg .mp4 .m4v .mkv .mov .wmv .ogv .divx .m2ts")
{
}

MediaDirectory::MediaDirectory(boost::filesystem::path p, Type type)
: _type(type),
 _path(p.string())
{
}

MediaDirectorySettings::pointer
MediaDirectorySettings::get(Wt::Dbo::Session& session)
{
	MediaDirectorySettings::pointer res;

	res = session.find<MediaDirectorySettings>().where("id = ?").bind(1);
	 // TODO bind necessary?
	if (!res)
		res = session.add( new MediaDirectorySettings());

	return res;
}

std::vector<boost::filesystem::path>
MediaDirectorySettings::getAudioFileExtensions(void) const
{
	return stringToPaths(_audioFileExtensions);
}

std::vector<boost::filesystem::path>
MediaDirectorySettings::getVideoFileExtensions(void) const
{
	return stringToPaths(_videoFileExtensions);
}

void
MediaDirectorySettings::setAudioFileExtensions(std::vector<boost::filesystem::path> extensions)
{
	_audioFileExtensions = pathsToString(extensions);
}

void
MediaDirectorySettings::setVideoFileExtensions(std::vector<boost::filesystem::path> extensions)
{
	_videoFileExtensions = pathsToString(extensions);
}


MediaDirectory::pointer
MediaDirectory::create(Wt::Dbo::Session& session, boost::filesystem::path p, Type type)
{
	return session.add( new MediaDirectory( p, type ) );
}

void
MediaDirectory::eraseAll(Wt::Dbo::Session& session)
{
	std::vector<MediaDirectory::pointer> dirs = getAll(session);
	BOOST_FOREACH(MediaDirectory::pointer dir, dirs)
		dir.remove();
}

std::vector<MediaDirectory::pointer>
MediaDirectory::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection< MediaDirectory::pointer > res = session.find<MediaDirectory>();

	return std::vector<MediaDirectory::pointer>(res.begin(), res.end());
}

std::vector<MediaDirectory::pointer>
MediaDirectory::getByType(Wt::Dbo::Session& session, Type type)
{
	Wt::Dbo::collection< MediaDirectory::pointer > res = session.find<MediaDirectory>().where("type = ?").bind (type);

	return std::vector<MediaDirectory::pointer>(res.begin(), res.end());
}

MediaDirectory::pointer
MediaDirectory::get(Wt::Dbo::Session& session, boost::filesystem::path p, Type type)
{
	return session.find<MediaDirectory>().where("path = ?").where("type = ?").bind( p.string()).bind(type);
}


} // namespace Database
