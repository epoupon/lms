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

#include <memory>

#include "database/objects/ArtistFeedbackId.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseFeedbackId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackFeedbackId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"

namespace lms::feedback
{
    class IFeedbackBackend
    {
    public:
        virtual ~IFeedbackBackend() = default;

        virtual void onFeedbackChanged(db::ArtistFeedbackId id) = 0;
        virtual void onFeedbackChanged(db::ReleaseFeedbackId id) = 0;
        virtual void onFeedbackChanged(db::TrackFeedbackId id) = 0;

        virtual bool canBeFeedbacked(db::ArtistId artistId) const = 0;
        virtual bool canBeFeedbacked(db::ReleaseId releaseId) const = 0;
        virtual bool canBeFeedbacked(db::TrackId trackId) const = 0;

        virtual void requestImmediateImport(db::UserId userId) = 0;
        virtual void requestImmediateExport() = 0;
    };

    std::unique_ptr<IFeedbackBackend> createFeedbackBackend(std::string_view backendName);
} // namespace lms::feedback
