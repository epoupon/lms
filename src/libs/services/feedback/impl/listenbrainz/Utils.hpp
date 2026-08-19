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

#include <optional>

#include "core/ILogger.hpp"

#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"

#define LOG(sev, message) LMS_LOG(FEEDBACK, sev, "[listenbrainz] " << message)

namespace lms::db
{
    class Session;
}

namespace lms::feedback::listenBrainz::utils
{
    std::string getListenBrainzToken(db::Session& session, db::UserId userId);
    std::string parseValidateToken(std::string_view msgBody);

    // See https://listenbrainz.readthedocs.io/en/latest/users/api/recordings.html
    int toLBScore(db::FeedbackValue value);
    std::optional<db::FeedbackValue> fromLBScore(int score);

    // ListenBrainz's recording-feedback endpoint only supports recordings that have a recording MBID
    bool canBeFeedbacked(db::Session& session, db::TrackId trackId);
} // namespace lms::feedback::listenBrainz::utils
