/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "Utils.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"
#include "utils/String.hpp"

#include "SubsonicId.hpp"

using namespace Database;

namespace API::Subsonic
{

std::string
getArtistNames(const std::vector<Artist::pointer>& artists)
{
	if (artists.size() == 1)
		return artists.front()->getName();

	std::vector<std::string> names;
	names.resize(artists.size());

	std::transform(std::cbegin(artists), std::cend(artists), std::begin(names),
			[](const Artist::pointer& artist)
			{
				return artist->getName();
			});

	return StringUtils::joinStrings(names, ", ");
}


std::string
makeNameFilesystemCompatible(const std::string& name)
{
	return StringUtils::replaceInString(name, "/", "_");
}

static
std::string
getTrackPath(const Track::pointer& track)
{
	std::string path;

	// The track path has to be relative from the root

	const auto release {track->getRelease()};
	if (release)
	{
		auto artists {release->getReleaseArtists()};
		if (artists.empty())
			artists = release->getArtists();

		if (artists.size() > 1)
			path = "Various Artists/";
		else if (artists.size() == 1)
			path = makeNameFilesystemCompatible(artists.front()->getName()) + "/";

		path += makeNameFilesystemCompatible(track->getRelease()->getName()) + "/";
	}

	if (track->getDiscNumber())
		path += std::to_string(*track->getDiscNumber()) + "-";
	if (track->getTrackNumber())
		path += std::to_string(*track->getTrackNumber()) + "-";

	path += makeNameFilesystemCompatible(track->getName());

	if (track->getPath().has_extension())
		path += track->getPath().extension();

	return path;
}

Response::Node
trackToResponseNode(const Track::pointer& track, Session& dbSession, const User::pointer& user)
{
	Response::Node trackResponse;

	trackResponse.setAttribute("id", IdToString({Id::Type::Track, track.id()}));
	trackResponse.setAttribute("isDir", "false");
	trackResponse.setAttribute("title", track->getName());
	if (track->getTrackNumber())
		trackResponse.setAttribute("track", std::to_string(*track->getTrackNumber()));
	if (track->getDiscNumber())
		trackResponse.setAttribute("discNumber", std::to_string(*track->getDiscNumber()));
	if (track->getYear())
		trackResponse.setAttribute("year", std::to_string(*track->getYear()));

	trackResponse.setAttribute("path", getTrackPath(track));
	{
		std::error_code ec;
		const auto fileSize {std::filesystem::file_size(track->getPath(), ec)};
		if (!ec)
			trackResponse.setAttribute("size", std::to_string(fileSize));
	}

	if (track->getPath().has_extension())
	{
		auto extension {track->getPath().extension()};
		trackResponse.setAttribute("suffix", extension.string().substr(1));
	}

	trackResponse.setAttribute("coverArt", IdToString({Id::Type::Track, track.id()}));

	auto artists {track->getArtists()};
	if (!artists.empty())
	{
		trackResponse.setAttribute("artist", getArtistNames(artists));

		if (artists.size() == 1)
			trackResponse.setAttribute("artistId", IdToString({Id::Type::Artist, artists.front().id()}));
	}

	if (track->getRelease())
	{
		trackResponse.setAttribute("album", track->getRelease()->getName());
		trackResponse.setAttribute("albumId", IdToString({Id::Type::Release, track->getRelease().id()}));
		trackResponse.setAttribute("parent", IdToString({Id::Type::Release, track->getRelease().id()}));
	}

	trackResponse.setAttribute("duration", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count()));
	trackResponse.setAttribute("type", "music");

	if (user && user->hasStarredTrack(track))
		trackResponse.setAttribute("starred", reportedStarredDate);

	// Report the first GENRE for this track
	ClusterType::pointer clusterType {ClusterType::getByName(dbSession, genreClusterName)};
	if (clusterType)
	{
		auto clusters {track->getClusterGroups({clusterType}, 1)};
		if (!clusters.empty() && !clusters.front().empty())
			trackResponse.setAttribute("genre", clusters.front().front()->getName());
	}

	return trackResponse;
}

}
