/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <optional>
#include <vector>

#include "DatabaseCollectorBase.hpp"

#include "database/Types.hpp"

namespace Database
{
	class Artist;
}

namespace UserInterface
{
	class ArtistCollector : public DatabaseCollectorBase
	{
		public:
			using DatabaseCollectorBase::DatabaseCollectorBase;

			std::vector<Wt::Dbo::ptr<Database::Artist>>		get(std::optional<Database::Range> range, bool& moreResults);
			void reset() { _randomArtists.clear(); }
			void setArtistLinkType(std::optional<Database::TrackArtistLinkType> linkType) { _linkType = linkType; }

		private:
			std::vector<Wt::Dbo::ptr<Database::Artist>> getRandomArtists(std::optional<Range> range, bool& moreResults);
			std::vector<Database::IdType> _randomArtists;
			std::optional<Database::TrackArtistLinkType> _linkType;
	};
} // ns UserInterface

