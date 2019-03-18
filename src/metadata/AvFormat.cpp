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

#include "AvFormat.hpp"

#include "av/AvInfo.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace MetaData
{

using MetadataMap = std::map<std::string, std::string>;

boost::optional<std::string>
findFirstValueOf(const MetadataMap& metadataMap, std::initializer_list<std::string> tags)
{
	auto it = std::find_first_of(std::cbegin(metadataMap), std::cend(metadataMap), std::cbegin(tags), std::cend(tags), [](const auto& it, const auto& str) { return it.first == str; });
	if (it == std::cend(metadataMap))
		return boost::none;

	return stringTrim(it->second);
}

static
boost::optional<Album>
getAlbum(const MetadataMap& metadataMap)
{
	boost::optional<Album> res;

	auto album {findFirstValueOf(metadataMap, {"ALBUM"})};
	if (!album)
		return res;

	res = Album{*album, ""};

	auto albumMBID {findFirstValueOf(metadataMap, {"MUSICBRAINZ ALBUM ID", "MUSICBRAINZ_ALBUMID"})};
	if (!albumMBID)
		return res;

	res->musicBrainzAlbumID = *albumMBID;

	return res;
}

static
boost::optional<Artist>
getAlbumArtist(const MetadataMap& metadataMap)
{
	boost::optional<Artist> res;

	auto artist {findFirstValueOf(metadataMap, {"ALBUM_ARTIST"})};
	if (!artist)
		return res;

	res = Artist{*artist, ""};

	auto artistMBID {findFirstValueOf(metadataMap, {"MUSICBRAINZ ALBUM ARTIST ID"})};
	if (!artistMBID)
		return res;

	res->musicBrainzArtistID = *artistMBID;

	return res;
}

static
std::vector<Artist>
getArtists(const MetadataMap& metadataMap)
{
	std::vector<Artist> artists;

	std::vector<std::string> artistNames;
	if (metadataMap.find("ARTISTS") != metadataMap.end())
	{
		artistNames = splitString(metadataMap.find("ARTISTS")->second, "/;");
	}
	else if (metadataMap.find("ARTIST") != metadataMap.end())
	{
		artistNames = {metadataMap.find("ARTIST")->second};
	}

	std::vector<std::string> artistMBIDs;
	{
		auto mbids {findFirstValueOf(metadataMap, {"MUSICBRAINZ ARTIST ID", "MUSICBRAINZ_ARTISTID"})};
		if (mbids)
			artistMBIDs = splitString(*mbids, "/");
	}

	for (std::size_t i {}; i < artistNames.size(); ++i)
	{
		Artist artist{std::move(artistNames[i]), ""};

		if (artistNames.size() == artistMBIDs.size())
			artist.musicBrainzArtistID = std::move(artistMBIDs[i]);

		artists.emplace_back(std::move(artist));
	}

	return artists;
}

boost::optional<Track>
AvFormat::parse(const boost::filesystem::path& p, bool debug)
{
	Track track;

	try
	{
		Av::MediaFile mediaFile {p};

		// Stream info
		{
			std::vector<AudioStream> audioStreams;

			for (auto stream : mediaFile.getStreamInfo())
			{
				MetaData::AudioStream audioStream {.bitRate = static_cast<unsigned>(stream.bitrate)};
				track.audioStreams.emplace_back(audioStream);
			}
		}

		track.duration = mediaFile.getDuration();
		track.hasCover = mediaFile.hasAttachedPictures();

		MetaData::Clusters clusters;

		const std::map<std::string, std::string> metadataMap {mediaFile.getMetaData()};

		for (const auto& metadata : metadataMap)
		{
			const std::string& tag {metadata.first};
			const std::string& value {metadata.second};

			if (debug)
				std::cout << "TAG = " << tag << ", VAL = " << value << std::endl;

			if (tag == "TITLE")
				track.title = value;
			else if (tag == "TRACK")
			{
				// Expecting 'Number/Total'
				std::vector<std::string> strings {splitString(value, "/") };

				if (strings.size() > 0)
				{
					track.trackNumber = readAs<std::size_t>(strings[0]);

					if (strings.size() > 1)
						track.totalTrack = readAs<std::size_t>(strings[1]);
				}
			}
			else if (tag == "DISC")
			{
				// Expecting 'Number/Total'
				std::vector<std::string> strings {splitString(value, "/")};

				if (strings.size() > 0)
				{
					track.discNumber = readAs<std::size_t>(strings[0]);

					if (strings.size() > 1)
						track.totalDisc = readAs<std::size_t>(strings[1]);
				}
			}
			else if (tag == "DATE"
					|| tag == "YEAR"
					|| tag == "WM/Year")
			{
				track.year = readAs<int>(value);
			}
			else if (tag == "TDOR"	// Original release time (ID3v2 2.4)
					|| tag == "TORY")	// Original release year
			{
				track.originalYear = readAs<int>(value);
			}
			else if (tag == "ACOUSTID ID")
			{
				track.acoustID = value;
			}
			else if (tag == "MUSICBRAINZ RELEASE TRACK ID"
					|| tag == "MUSICBRAINZ_RELEASETRACKID"
					|| tag == "MUSICBRAINZ_TRACKID")
			{
				track.musicBrainzTrackID = value;
			}
			else if (_clusterTypeNames.find(tag) != _clusterTypeNames.end())
			{
				std::vector<std::string> clusterNames {splitString(value, "/,;")};

				if (!clusterNames.empty())
					track.clusters[tag] = std::set<std::string>{clusterNames.begin(), clusterNames.end()};
			}

		}

		track.artists = getArtists(metadataMap);
		track.album = getAlbum(metadataMap);
		track.albumArtist = getAlbumArtist(metadataMap);
	}
	catch(Av::MediaFileException& e)
	{
		return boost::none;
	}

	return track;
}

} // namespace MetaData

