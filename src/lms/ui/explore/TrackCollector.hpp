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

#include "services/database/Object.hpp"
#include "services/database/TrackId.hpp"
#include "DatabaseCollectorBase.hpp"

namespace Database
{
	class Track;
}

namespace UserInterface
{
	class TrackCollector : public DatabaseCollectorBase
	{
		public:
			using DatabaseCollectorBase::DatabaseCollectorBase;

			Database::RangeResults<Database::TrackId>	get(Database::Range range);
			void reset() { _randomTracks.reset(); }

		private:
			Database::RangeResults<Database::TrackId> getRandomTracks(Range range);
			std::optional<Database::RangeResults<Database::TrackId>> _randomTracks;
	};
} // ns UserInterface

