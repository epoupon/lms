/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "IFeedbackBackend.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::feedback
{
    class InternalBackend final : public IFeedbackBackend
    {
    public:
        InternalBackend(db::IDb& db);
        ~InternalBackend() override = default;
        InternalBackend(const InternalBackend&) = delete;
        InternalBackend& operator=(const InternalBackend&) = delete;

    private:
        void onStarred(db::StarredArtistId artistId) override;
        void onUnstarred(db::StarredArtistId artistId) override;
        void onStarred(db::StarredReleaseId releaseId) override;
        void onUnstarred(db::StarredReleaseId releaseId) override;
        void onStarred(db::StarredTrackId trackId) override;
        void onUnstarred(db::StarredTrackId trackId) override;

        db::IDb& _db;
    };
} // namespace lms::feedback
