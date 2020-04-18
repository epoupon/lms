/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>

namespace Database {

class Session;

class SubsonicSettings : public Wt::Dbo::Dbo<SubsonicSettings>
{
	public:
		using pointer = Wt::Dbo::ptr<SubsonicSettings>;

		enum class ArtistListMode
		{
			AllArtists,
			ReleaseArtists,
		};

		static inline constexpr bool defaultSubsonicAPIEnabled {true};
		static inline constexpr ArtistListMode defaultArtistListMode {ArtistListMode::AllArtists};

		static void init(Session& session);

		static pointer get(Session& session);


		// Getters
		bool		isAPIEnabled() const { return _isAPIEnabled; }
		ArtistListMode	getArtistListMode() const { return _artistListMode; }

		// Setters
		void setAPIEnabled(bool enabled) { _isAPIEnabled = enabled; }
		void setArtistListMode(ArtistListMode mode) {_artistListMode = mode; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _isAPIEnabled,	"api_enabled");
			Wt::Dbo::field(a, _artistListMode,	"artist_list_mode");
		}

	private:

		bool		_isAPIEnabled {defaultSubsonicAPIEnabled};
		ArtistListMode	_artistListMode {defaultArtistListMode};
};

} // namespace Database

