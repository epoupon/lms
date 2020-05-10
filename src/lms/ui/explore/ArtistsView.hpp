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

#include <memory>
#include <vector>

#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>
#include <Wt/WSignal.h>

#include "database/Types.hpp"

namespace Database
{
	class Artist;
}

namespace UserInterface {

class Filters;

class Artists : public Wt::WTemplate
{
	public:
		Artists(Filters* filters);

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

		std::vector<Wt::Dbo::ptr<Database::Artist>> getArtists(std::optional<Database::Range> range, bool& moreResults);
		std::vector<Wt::Dbo::ptr<Database::Artist>> getRandomArtists(std::optional<Database::Range> range, bool& moreResults);

		static constexpr Mode defaultMode {Mode::Random};
		static constexpr std::size_t batchSize {30};
		static inline std::unordered_map<Mode, std::optional<std::size_t>> maxItemsPerMode
		{
			{Mode::Random, batchSize * 4},
			{Mode::RecentlyPlayed, batchSize * 4},
			{Mode::RecentlyAdded, batchSize * 2},
			{Mode::MostPlayed, batchSize * 2},
			{Mode::All, std::nullopt},
		};

		Mode _mode {defaultMode};
		std::vector<Database::IdType> _randomArtists;
		Filters* _filters {};
		Wt::WPushButton* _showMore {};
		Wt::WContainerWidget* _container {};
		Wt::WComboBox* _linkType {};
};

} // namespace UserInterface

