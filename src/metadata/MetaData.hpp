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
#include <vector>

namespace MetaData
{
	using Clusters = std::map<std::string /* type */, std::set<std::string> /* names */>;

	struct Artist
	{
		std::string name;
		std::string musicBrainzArtistID;
	};

	struct Album
	{
		std::string name;
		std::string musicBrainzAlbumID;
	};

	struct AudioStream
	{
		unsigned bitRate;
	};

	struct Track
	{
		std::vector<Artist>		artists;
		std::vector<Artist>		albumArtists;
		std::string			title;
		std::string			musicBrainzTrackID;
		std::string			musicBrainzRecordID;
		std::optional<Album>		album;
		Clusters			clusters;
		std::chrono::milliseconds 	duration {};
		std::optional<std::size_t>	trackNumber;
		std::optional<std::size_t>	totalTrack;
		std::optional<std::size_t>	discNumber;
		std::optional<std::size_t>	totalDisc;
		std::optional<int>		year;
		std::optional<int>		originalYear;
		bool				hasCover {false};
		std::vector<AudioStream>	audioStreams;
		std::string			acoustID;
		std::string			copyright;
		std::string			copyrightURL;
	};

	class Parser
	{
		public:
			virtual std::optional<Track> parse(const std::filesystem::path& p, bool debug = false) = 0;

			void setClusterTypeNames(const std::set<std::string>& clusterTypeNames) { _clusterTypeNames = clusterTypeNames; }

		protected:
			std::set<std::string> _clusterTypeNames;
	};

} // namespace MetaData

