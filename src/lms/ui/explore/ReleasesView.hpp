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

#include <Wt/WContainerWidget.h>

#include "services/database/Types.hpp"

#include "common/Template.hpp"
#include "PlayQueueAction.hpp"
#include "ReleaseCollector.hpp"

namespace UserInterface
{
	class Filters;
	class InfiniteScrollingContainer;

	class Releases : public Template
	{
		public:
			Releases(Filters& filters);

			PlayQueueActionReleaseSignal releasesAction;

		private:
			void refreshView();
			void refreshView(ReleaseCollector::Mode mode);

			void addSome();
			std::vector<Database::ReleaseId> getAllReleases();

			static constexpr std::size_t _maxItemsPerLine {6};
			static constexpr std::size_t _batchSize {_maxItemsPerLine};
			static constexpr std::size_t _maxCount {_maxItemsPerLine * 500};

			Wt::WWidget*				_currentActiveItem {};
			InfiniteScrollingContainer* _container {};
			ReleaseCollector			_releaseCollector;
			static constexpr ReleaseCollector::Mode _defaultMode {ReleaseCollector::Mode::Random};
	};
} // namespace UserInterface

