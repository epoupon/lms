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

#include <map>
#include <set>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

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
	}

	struct AudioStream
	{
		unsigned bitRate;
	};

	struct Track
	{
		std::vector<Artist>		artists;
		boost::optional<Artist>		albumArtist;
		std::string			title;
		std::string			musicBrainzTrackID;
		std::string			musicBrainzRecordID;
		boost::optional<Album>		album;
		Clusters			clusters;
		std::chrono::milliseconds 	duration {};
		boost::optional<std::size_t>	trackNumber;
		boost::optional<std::size_t>	totalTrack;
		boost::optional<std::size_t>	discNumber;
		boost::optional<std::size_t>	totalDisc;
		boost::optional<int>		year;
		boost::optional<int>		originalYear;
		bool				hasCover {false};
		std::vector<AudioStream>	audioStreams;
		std::string			acoustId;
		std::string			copyright;
		std::string			copyrightURL;
	};

	class Parser
	{
		public:
			virtual boost::optional<Track> parse(const boost::filesystem::path& p, bool debug = false) = 0;

			void setClusterTypeNames(const std::set<std::string>& clusterTypeNames) { _clusterTypeNames = clusterTypeNames; }

		protected:
			std::set<std::string> _clusterTypeNames;
	};

} // namespace MetaData

