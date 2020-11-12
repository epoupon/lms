/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "utils/UUID.hpp"

namespace MetaData
{
	using Clusters = std::map<std::string /* type */, std::set<std::string> /* names */>;

	struct Artist
	{
		std::string name;
		std::optional<std::string> sortName;
		std::optional<UUID> musicBrainzArtistID;

		Artist(std::string_view _name) : name {_name} {}
		Artist(std::string_view _name, std::optional<std::string> _sortName, std::optional<UUID> _musicBrainzArtistID) : name {_name}, sortName {_sortName}, musicBrainzArtistID {_musicBrainzArtistID} {}
	};

	struct Album
	{
		std::string name;
		std::optional<UUID> musicBrainzAlbumID;
	};

	struct AudioStream
	{
		unsigned bitRate;
	};

	struct Track
	{
		std::vector<Artist>		artists;
		std::vector<Artist>		albumArtists;
		std::string				title;
		std::optional<UUID>		musicBrainzTrackID;
		std::optional<UUID>		musicBrainzRecordID;
		std::optional<Album>	album;
		Clusters				clusters;
		std::chrono::milliseconds 	duration;
		std::optional<std::size_t>	trackNumber;
		std::optional<std::size_t>	totalTrack;
		std::optional<std::size_t>	discNumber;
		std::optional<std::size_t>	totalDisc;
		std::optional<int>		year;
		std::optional<int>		originalYear;
		bool					hasCover {};
		std::vector<AudioStream>	audioStreams;
		std::optional<UUID>		acoustID;
		std::string				copyright;
		std::string				copyrightURL;
		std::optional<float>	trackReplayGain;
		std::optional<float>	albumReplayGain;
		std::string				discSubtitle;
		std::vector<Artist>		conductorArtists;
		std::vector<Artist>		composerArtists;
		std::vector<Artist>		lyricistArtists;
		std::vector<Artist>		mixerArtists;
		std::vector<Artist>		producerArtists;
		std::vector<Artist>		remixerArtists;
	};

	class IParser
	{
		public:
			virtual ~IParser() = default;

			virtual std::optional<Track> parse(const std::filesystem::path& p, bool debug = false) = 0;

			void setClusterTypeNames(const std::set<std::string>& clusterTypeNames) { _clusterTypeNames = clusterTypeNames; }

		protected:
			std::set<std::string> _clusterTypeNames;
	};

} // namespace MetaData

