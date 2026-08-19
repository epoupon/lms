/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"

#include "database/objects/UserId.hpp"

#define LMS_LOG_LASTFM(sev, message) LMS_LOG(SCROBBLING, sev, "[lastfm] " << message)

namespace lms::db
{
    class Session;
}

namespace lms::scrobbling::lastFm::utils
{
    struct LastFmCredentials
    {
        std::string apiKey;
        std::string apiSecret;
        std::string sessionKey;
    };

    LastFmCredentials getLastFmCredentials(db::Session& session, db::UserId userId);

    std::string computeApiSig(const std::map<std::string, std::string>& params, std::string_view secret);
    std::string buildFormBody(const std::map<std::string, std::string>& params);
    std::string parseAuthToken(std::string_view msgBody);
    std::string parseSessionKey(std::string_view msgBody);

    enum class ScrobbleIgnoredCode
    {
        None = 0, // accepted
        ArtistIgnored = 1,
        TrackIgnored = 2,
        TimestampTooOld = 3,
        TimestampTooNew = 4,
        DailyLimitExceeded = 5,
    };

    core::LiteralString toString(ScrobbleIgnoredCode code);

    struct ScrobbleResult
    {
        ScrobbleIgnoredCode ignoredCode{ ScrobbleIgnoredCode::None };
        std::string ignoredMessage; // Last.fm's own explanation, if ignored
    };

    // One entry per submitted scrobble, in order.
    // Empty if the response couldn't be parsed or didn't yield exactly expectedCount entries.
    std::vector<ScrobbleResult> parseScrobbleResults(std::string_view msgBody, std::size_t expectedCount);
} // namespace lms::scrobbling::lastFm::utils
