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

#include "Utils.hpp"

#include <iomanip>
#include <sstream>

#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>

#include "core/Md5.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"

namespace lms::scrobbling::lastFm::utils
{
    LastFmCredentials getLastFmCredentials(db::Session& session, db::UserId userId)
    {
        LastFmCredentials creds;

        auto transaction{ session.createReadTransaction() };
        if (const db::User::pointer user{ db::User::find(session, userId) })
        {
            creds.apiKey = user->getLastFmApiKey();
            creds.apiSecret = user->getLastFmApiSecret();
            creds.sessionKey = user->getLastFmSessionKey();
        }

        return creds;
    }

    std::string computeApiSig(const std::map<std::string, std::string>& params, std::string_view secret)
    {
        std::string payload;
        for (const auto& [key, value] : params)
        {
            if (key == "format" || key == "callback")
                continue;
            payload += key;
            payload += value;
        }
        payload += secret;

        const auto digest{ core::md5(payload) };

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (const std::byte b : digest)
            oss << std::setw(2) << static_cast<int>(b);

        return oss.str();
    }

    std::string buildFormBody(const std::map<std::string, std::string>& params)
    {
        std::string body;
        bool first{ true };

        for (const auto& [key, value] : params)
        {
            if (!first)
                body += '&';
            first = false;
            body += core::stringUtils::urlEncode(key);
            body += '=';
            body += core::stringUtils::urlEncode(value);
        }

        return body;
    }

    std::string parseAuthToken(std::string_view msgBody)
    {
        Wt::Json::ParseError error;
        Wt::Json::Object root;
        if (!Wt::Json::parse(std::string{ msgBody }, root, error))
        {
            LOG(ERROR, "Cannot parse auth.getToken response: " << error.what());
            return {};
        }

        return static_cast<std::string>(root.get("token").orIfNull(""));
    }

    std::string parseSessionKey(std::string_view msgBody)
    {
        Wt::Json::ParseError error;
        Wt::Json::Object root;
        if (!Wt::Json::parse(std::string{ msgBody }, root, error))
        {
            LOG(ERROR, "Cannot parse auth.getSession response: " << error.what());
            return {};
        }

        try
        {
            const Wt::Json::Object& session{ static_cast<const Wt::Json::Object&>(root.get("session")) };
            return static_cast<std::string>(session.get("key").orIfNull(""));
        }
        catch (const Wt::WException& e)
        {
            LOG(ERROR, "Cannot extract session key: " << e.what());
            return {};
        }
    }
} // namespace lms::scrobbling::lastFm::utils
