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
#include <Wt/Dbo/ptr.h>

namespace Database
{
	using IdType = Wt::Dbo::dbo_default_traits::IdType;

	static inline bool IdIsValid(IdType id)
	{
		return id != Wt::Dbo::dbo_default_traits::invalidId();
	}

	struct Range
	{
		std::size_t offset {};
		std::size_t limit {};
	};

	enum class TrackArtistLinkType
	{
		Artist,	// regular artist
		Arranger,
		Composer,
		Conductor,
		Lyricist,
		Mixer,
		Performer,
		Producer,
		ReleaseArtist,
		Remixer,
		Writer,
	};

	// User selectable audio file formats
	// Do not change values
	enum class AudioFormat
	{
		MP3		= 1,
		OGG_OPUS	= 2,
		OGG_VORBIS	= 3,
		WEBM_VORBIS	= 4,
		MATROSKA_OPUS	= 5,
	};
	using Bitrate = std::uint32_t;

	// Do not change enum values!
	enum class Scrobbler
	{
		Internal		= 0,
		ListenBrainz	= 1,
	};
}

