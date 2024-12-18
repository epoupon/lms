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

#include "ReleaseCollector.hpp"
#include "common/Template.hpp"

namespace lms::ui
{
    class Filters;
    class InfiniteScrollingContainer;
    class PlayQueueController;

    class Releases : public Template
    {
    public:
        Releases(Filters& filters, PlayQueueController& playQueueController);

    private:
        void refreshView();
        void refreshView(ReleaseCollector::Mode mode);
        void refreshView(const Wt::WString& searchText);

        void addSome();
        std::vector<db::ReleaseId> getAllReleases();

        static constexpr std::size_t _maxItemsPerLine{ 6 };
        static constexpr std::size_t _batchSize{ _maxItemsPerLine };
        static constexpr std::size_t _maxCount{ _maxItemsPerLine * 500 };

        PlayQueueController& _playQueueController;
        InfiniteScrollingContainer* _container{};
        ReleaseCollector _releaseCollector;
        static constexpr ReleaseCollector::Mode _defaultMode{ ReleaseCollector::Mode::Random };
    };
} // namespace lms::ui
