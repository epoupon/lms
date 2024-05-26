/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "database/StarredArtistId.hpp"
#include "database/StarredReleaseId.hpp"
#include "database/StarredTrackId.hpp"

namespace lms::feedback
{
    class IFeedbackBackend
    {
    public:
        virtual ~IFeedbackBackend() = default;

        virtual void onStarred(db::StarredArtistId) = 0;
        virtual void onUnstarred(db::StarredArtistId) = 0;
        virtual void onStarred(db::StarredReleaseId) = 0;
        virtual void onUnstarred(db::StarredReleaseId) = 0;
        virtual void onStarred(db::StarredTrackId) = 0;
        virtual void onUnstarred(db::StarredTrackId) = 0;
    };

    std::unique_ptr<IFeedbackBackend> createFeedbackBackend(std::string_view backendName);

} // namespace lms::feedback
