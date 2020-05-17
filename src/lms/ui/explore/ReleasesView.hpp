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

#include <optional>

#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "database/Types.hpp"
#include "PlayQueueAction.hpp"

namespace Database
{
	class Release;
}

namespace UserInterface {

class Filters;

class Releases : public Wt::WTemplate
{
	public:
		Releases(Filters* filters);

		Wt::Signal<PlayQueueAction, const std::vector<Database::IdType>&> releasesAction;

	private:

		enum class Mode
		{
			Random,
			RecentlyPlayed,
			RecentlyAdded,
			MostPlayed,
			All
		};

		void refreshView();
		void refreshView(Mode mode);
		void addSome();
		std::vector<Wt::Dbo::ptr<Database::Release>> getReleases(std::optional<Database::Range> range, bool& moreResults);
		std::vector<Wt::Dbo::ptr<Database::Release>> getRandomReleases(std::optional<Database::Range> range, bool& moreResults);
		std::vector<Database::IdType> getAllReleases();

		static constexpr Mode defaultMode {Mode::Random};
		static constexpr std::size_t batchSize {20};
		static inline std::unordered_map<Mode, std::optional<std::size_t>> maxItemsPerMode
		{
			{Mode::Random, batchSize * 6},
			{Mode::RecentlyPlayed, batchSize * 3},
			{Mode::RecentlyAdded, batchSize * 2},
			{Mode::MostPlayed, batchSize * 2},
			{Mode::All, batchSize * 30},
		};

		Mode _mode {defaultMode};
		Filters* _filters {};
		std::vector<Database::IdType> _randomReleases;
		Wt::WContainerWidget* _container {};
		Wt::WPushButton* _showMore {};
};

} // namespace UserInterface

