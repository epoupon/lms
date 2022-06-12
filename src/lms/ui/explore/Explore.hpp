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

#include <Wt/WTemplate.h>

#include "services/database/Types.hpp"
#include "PlayQueueAction.hpp"

namespace UserInterface
{
	class Filters;
	class SearchView;

	class Explore : public Wt::WTemplate
	{
		public:
			Explore(Filters* filters);

			void search(const Wt::WString& searchText);

			PlayQueueActionTrackSignal tracksAction;
			void setMaxTrackCountForAction(std::size_t maxTrackCount) { _maxTrackCount = maxTrackCount; }

		private:
			void handleArtistsAction(PlayQueueAction action, const std::vector<Database::ArtistId>& artistsId);
			void handleReleasesAction(PlayQueueAction action, const std::vector<Database::ReleaseId>& releasesId);
			void handleTracksAction(PlayQueueAction action, const std::vector<Database::TrackId>& tracksId);

			Filters* _filters {};
			SearchView* _search {};
			std::size_t _maxTrackCount {};
	};
} // namespace UserInterface

