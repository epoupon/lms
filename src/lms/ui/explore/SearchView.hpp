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

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include <Wt/WContainerWidget.h>
#include <Wt/WMenu.h>
#include <Wt/WTemplate.h>

#include "ArtistCollector.hpp"
#include "ReleaseCollector.hpp"
#include "TrackCollector.hpp"
#include "PlayQueueAction.hpp"

namespace UserInterface
{

	class InfiniteScrollingContainer;
	class Filters;

	class SearchView : public Wt::WTemplate
	{
		public:
			SearchView(Filters* filters);

			PlayQueueActionSignal tracksAction;

			void refreshView(const Wt::WString& searchText);

		private:

			// same order as in the menu
			enum class Mode
			{
				Release,
				Artist,
				Track,
			};

			std::size_t modeToIndex(Mode mode) const;
			Wt::WMenuItem& getItemMenu(Mode mode) const;
			InfiniteScrollingContainer& getResultContainer(Mode mode) const;

			static constexpr Mode _defaultMode {Mode::Release};
			static inline std::unordered_map<Mode, std::size_t> _batchSizes
			{
				{Mode::Artist, 6},
				{Mode::Release, 6},
				{Mode::Track, 6},
			};
			static inline std::unordered_map<Mode, std::size_t> _maxCounts
			{
				{Mode::Artist, 50},
				{Mode::Release, 60},
				{Mode::Track, 50},
			};
			std::size_t getBatchSize(Mode mode) const;
			std::size_t getMaxCount(Mode mode) const;

			void refreshView();
			void addSomeReleases();
			void addSomeArtists();
			void addSomeTracks();

			Filters* _filters {};
			Wt::WMenu* _menu {};
			ReleaseCollector	_releaseCollector;
			ArtistCollector		_artistCollector;
			TrackCollector		_trackCollector;

			std::vector<InfiniteScrollingContainer*> _results;
	};

} // namespace UserInterface

