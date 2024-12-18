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

#include <Wt/WComboBox.h>
#include <Wt/WTemplate.h>

#include "database/Types.hpp"

#include "ArtistCollector.hpp"
#include "common/Template.hpp"

namespace lms::ui
{
    class Filters;
    class InfiniteScrollingContainer;

    class Artists : public Template
    {
    public:
        Artists(Filters& filters);

    private:
        void refreshView();
        void refreshView(ArtistCollector::Mode mode);
        void refreshView(std::optional<db::TrackArtistLinkType> linkType);
        void refreshView(const Wt::WString& searchText);
        void addSome();

        static constexpr std::size_t _batchSize{ 30 };
        static constexpr std::size_t _maxCount{ 8000 };

        Wt::WWidget* _currentLinkTypeActiveItem{};
        InfiniteScrollingContainer* _container{};
        ArtistCollector _artistCollector;
        static constexpr ArtistCollector::Mode _defaultSortMode{ ArtistCollector::Mode::Random };
    };
} // namespace lms::ui
