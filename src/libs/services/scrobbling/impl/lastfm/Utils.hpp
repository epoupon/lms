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

#include "core/ILogger.hpp"

#include "database/objects/UserId.hpp"

#define LOG(sev, message) LMS_LOG(SCROBBLING, sev, "[lastfm] " << message)

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
} // namespace lms::scrobbling::lastFm::utils
