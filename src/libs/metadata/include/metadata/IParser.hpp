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
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WDate.h>
#include "utils/EnumSet.hpp"
#include "utils/UUID.hpp"

namespace MetaData
{
	using Tags = std::map<std::string /* type */, std::set<std::string> /* names */>;

	// Very simplified version of https://musicbrainz.org/doc/MusicBrainz_Database/Schema

	struct Artist
	{
		std::optional<UUID>			mbid;
		std::string					name;
		std::optional<std::string>	sortName;

		Artist(std::string_view _name) : name {_name} {}
		Artist(std::optional<UUID> _mbid, std::string_view _name, std::optional<std::string> _sortName) : mbid {std::move(_mbid)}, name {_name}, sortName {std::move(_sortName)} {}
	};

	using PerformerContainer = std::map<std::string /*role*/, std::vector<Artist>>;

	struct Release
	{
		// see https://musicbrainz.org/doc/Release_Group/Type
		enum class PrimaryType
		{
			Album,
			Single,
			EP,
			Broadcast,
			Other
		};

		enum class SecondaryType
		{
			Compilation,
			Soundtrack,
			Spokenword,
			Interview,
			Audiobook,
			AudioDrama,
			Live,
			Remix,
			DJMix,
			Mixtape_Street,
			Demo,
		};

		std::optional<UUID> 		mbid;
		std::string					name;
		std::string					artistDisplayName;
		std::vector<Artist>			artists;
		std::optional<std::size_t>	mediumCount;
		std::optional<PrimaryType>	primaryType;
		EnumSet<SecondaryType>		secondaryTypes;
	};

	struct Medium
	{
		std::string					type;
		std::string					name;
		std::optional<Release>		release;
		std::optional<std::size_t>	position; // in release
		std::optional<std::size_t>  trackCount;
		std::optional<float>        replayGain;
	};

	struct AudioStream
	{
		unsigned bitRate;
	};

	struct Track
	{
		std::optional<UUID>			mbid;
		std::optional<UUID>			recordingMBID;
		std::string					title;
		std::optional<Medium>		medium;
		std::optional<std::size_t>	position; // in medium
		Tags						tags;
		std::chrono::milliseconds 	duration;
		Wt::WDate					date;
		Wt::WDate					originalDate;
		bool						hasCover {};
		std::vector<AudioStream>	audioStreams;
		std::optional<UUID>			acoustID;
		std::string					copyright;
		std::string					copyrightURL;
		std::optional<float>		replayGain;
		std::string					artistDisplayName;
		std::vector<Artist>			artists;
		std::vector<Artist>			conductorArtists;
		std::vector<Artist>			composerArtists;
		std::vector<Artist>			lyricistArtists;
		std::vector<Artist>			mixerArtists;
		PerformerContainer			performerArtists;
		std::vector<Artist>			producerArtists;
		std::vector<Artist>			remixerArtists;
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

	enum class ParserType
	{
		TagLib,
		AvFormat,
	};

	enum class ParserReadStyle
	{
		Fast,
		Average,
		Accurate,
	};
	std::unique_ptr<IParser> createParser(ParserType parserType, ParserReadStyle parserReadStyle);
} // namespace MetaData

