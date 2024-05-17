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

#include "database/TrackId.hpp"
#include "database/ArtistId.hpp"
#include "database/ReleaseId.hpp"
#include "DatabaseCollectorBase.hpp"


namespace lms::ui
{
	using MediumId = std::variant<db::TrackId, db::ArtistId, db::ReleaseId>;

	class MultisearchCollector : public DatabaseCollectorBase
	{
		public:
			using DatabaseCollectorBase::DatabaseCollectorBase;

			[[nodiscard]] db::RangeResults<MediumId> get(const std::optional<db::Range>& requestedRange = std::nullopt) const ;

		private:
			[[nodiscard]] db::RangeResults<db::TrackId> getTracks(const std::optional<db::Range>& requestedRange) const ;
			[[nodiscard]] db::RangeResults<db::ReleaseId> getReleases(const std::optional<db::Range>& requestedRange) const ;
			[[nodiscard]] db::RangeResults<db::ArtistId> getArtists(const std::optional<db::Range>& requestedRange) const ;
	};
} // ns UserInterface

