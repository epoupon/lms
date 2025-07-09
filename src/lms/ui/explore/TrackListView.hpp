/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "database/objects/TrackListId.hpp"

#include "common/Template.hpp"

namespace lms::ui
{
    class Filters;
    class InfiniteScrollingContainer;
    class PlayQueueController;

    class TrackList : public Template
    {
    public:
        TrackList(Filters& filters, PlayQueueController& playQueueController);

        Wt::Signal<db::TrackListId> trackListDeleted;

    private:
        void refreshView();
        void addSome();

        static constexpr std::size_t _batchSize{ 6 };
        static constexpr std::size_t _maxCount{ 8000 };

        Filters& _filters;
        PlayQueueController& _playQueueController;
        db::TrackListId _trackListId;
        InfiniteScrollingContainer* _container{};
    };
} // namespace lms::ui
