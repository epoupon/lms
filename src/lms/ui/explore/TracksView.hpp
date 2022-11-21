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

#include "services/database/Types.hpp"

#include "common/Template.hpp"
#include "TrackCollector.hpp"

namespace UserInterface
{
	class Filters;
	class InfiniteScrollingContainer;
	class PlayQueueController;

	class Tracks : public Template
	{
		public:
			Tracks(Filters& filters, PlayQueueController& playQueueController);

		private:
			void refreshView();
			void refreshView(TrackCollector::Mode mode);
			void addSome();

			std::vector<Database::TrackId> getAllTracks();

			static constexpr TrackCollector::Mode _defaultMode {TrackCollector::Mode::Random};
			static constexpr std::size_t _batchSize {6};
			static constexpr std::size_t _maxCount {8000};

			Filters&					_filters;
			PlayQueueController&		_playQueueController;
			Wt::WWidget*				_currentActiveItem {};
			InfiniteScrollingContainer* _container {};
			TrackCollector				_trackCollector;
	};
} // namespace UserInterface

