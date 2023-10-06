/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <cstdint>
#include <cassert>
#include <functional>
#include <Wt/WDate.h>

namespace Database
{
	// Caution: do not change enum values if they are set!

	// Request:
	// 	  size = 0 => no size limit!
	// Response (via RangeResults)
	//    size => results size
	struct Range
	{
		std::size_t offset {};
		std::size_t size {};

		// TODO remove this
		operator bool() const { return size != 0; }
	};

	template <typename T>
	struct RangeResults
	{
		Range range;
		std::vector<T> results;
		bool moreResults;

		RangeResults getSubRange(Range subRange)
		{
			assert(subRange.offset >= range.offset);

			if (!subRange.size)
				subRange.size = range.size - (subRange.offset - range.offset);

			subRange.offset = std::min(subRange.offset, range.offset + range.size);
			subRange.size = std::min(subRange.size, range.offset + range.size - subRange.offset);

			RangeResults subResults;

			auto itBegin {std::cbegin(results) + subRange.offset - range.offset};
			auto itEnd {itBegin + subRange.size};
			subResults.results.reserve(std::distance(itBegin, itEnd));
			std::copy(itBegin, itEnd, std::back_inserter(subResults.results));

			subResults.range = subRange;
			if (subRange.offset + subRange.size == range.offset + range.size)
				subResults.moreResults = moreResults;
			else
				subResults.moreResults = true;

			return subResults;
		}
	};

	struct DateRange
	{
		Wt::WDate begin;
		Wt::WDate end;

		static DateRange fromYearRange(int from, int to);
	};

	struct DiscInfo
	{
		std::size_t position;
		std::string name;
	};

	enum class ArtistSortMethod
	{
		None,
		ByName,
		BySortName,
		Random,
		LastWritten,
		StarredDateDesc,
	};

	enum class ReleaseSortMethod
	{
		None,
		Name,
		Date,
		OriginalDate,
		OriginalDateDesc,
		Random,
		LastWritten,
		StarredDateDesc,
	};

	enum class TrackListSortMethod
	{
		None,
		Name,
		LastModifiedDesc,
	};

	enum class TrackSortMethod
	{
		None,
		Random,
		LastWritten,
		StarredDateDesc,
		Name,
		DateDescAndRelease,
		Release, // order by disc/track number
		TrackList, // order by asc order in tracklist
	};

	enum class TrackArtistLinkType
	{
		Artist		= 0,	// regular track artist
		Arranger	= 1,
		Composer	= 2,
		Conductor	= 3,
		Lyricist	= 4,
		Mixer		= 5,
		Performer	= 6,
		Producer	= 7,
		ReleaseArtist = 8,
		Remixer		= 9,
		Writer		= 10,
	};

	// User selectable audio file formats
	enum class AudioFormat
	{
		MP3				= 1,
		OGG_OPUS		= 2,
		OGG_VORBIS		= 3,
		WEBM_VORBIS		= 4,
		MATROSKA_OPUS	= 5,
	};

	using Bitrate = std::uint32_t;
	// Do not remove values!
	void visitAllowedAudioBitrates(std::function<void(Bitrate)>);
	bool isAudioBitrateAllowed(Bitrate bitrate);

	enum class Scrobbler
	{
		Internal		= 0,
		ListenBrainz	= 1,
	};

	enum class ScrobblingState
	{
		PendingAdd		= 0,
		Synchronized	= 1,
		PendingRemove	= 2,
	};

	enum class UserType
	{
		REGULAR	= 0,
		ADMIN	= 1,
		DEMO	= 2,
	};

	enum class UITheme
	{
		Light	= 0,
		Dark	= 1,
	};

	enum class SubsonicArtistListMode
	{
		AllArtists		= 0,
		ReleaseArtists	= 1,
		TrackArtists	= 2,
	};

	enum class TrackListType
	{
		Playlist,  // user controlled playlists
		Internal,  // internal usage (current playqueue, history, ...)
	};

	// as defined in https://musicbrainz.org/doc/Release_Group/Type
	enum class ReleaseTypePrimary
	{
		Album,
		Single,
		EP,
		Broadcast,
		Other,
	};

	enum class ReleaseTypeSecondary
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
}

