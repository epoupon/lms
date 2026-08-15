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

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>

#include "core/Md5.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"

namespace lms::scrobbling::lastFm::utils
{
    namespace
    {
        ScrobbleResult parseScrobbleResult(const Wt::Json::Object& scrobble)
        {
            ScrobbleResult res;

            if (scrobble.type("ignoredMessage") != Wt::Json::Type::Object)
                return res;

            const Wt::Json::Object& ignoredMessage{ static_cast<const Wt::Json::Object&>(scrobble.get("ignoredMessage")) };
            const std::string codeStr{ static_cast<std::string>(ignoredMessage.get("code").orIfNull("0")) };

            res.ignoredCode = core::stringUtils::readAs<ScrobbleIgnoredCode>(codeStr).value_or(ScrobbleIgnoredCode::None);
            res.ignoredMessage = static_cast<std::string>(ignoredMessage.get("#text").orIfNull(""));

            return res;
        }
    } // namespace

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
            LMS_LOG_LASTFM(ERROR, "Cannot parse auth.getToken response: " << error.what());
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
            LMS_LOG_LASTFM(ERROR, "Cannot parse auth.getSession response: " << error.what());
            return {};
        }

        try
        {
            const Wt::Json::Object& session{ static_cast<const Wt::Json::Object&>(root.get("session")) };
            return static_cast<std::string>(session.get("key").orIfNull(""));
        }
        catch (const Wt::WException& e)
        {
            LMS_LOG_LASTFM(ERROR, "Cannot extract session key: " << e.what());
            return {};
        }
    }

    core::LiteralString toString(ScrobbleIgnoredCode code)
    {
        switch (code)
        {
        case ScrobbleIgnoredCode::None:
            return "none";
        case ScrobbleIgnoredCode::ArtistIgnored:
            return "artist ignored";
        case ScrobbleIgnoredCode::TrackIgnored:
            return "track ignored";
        case ScrobbleIgnoredCode::TimestampTooOld:
            return "timestamp too old";
        case ScrobbleIgnoredCode::TimestampTooNew:
            return "timestamp too new";
        case ScrobbleIgnoredCode::DailyLimitExceeded:
            return "daily limit exceeded";
        }

        return "unknown";
    }

    std::vector<ScrobbleResult> parseScrobbleResults(std::string_view msgBody, std::size_t expectedCount)
    {
        Wt::Json::ParseError error;
        Wt::Json::Object root;
        if (!Wt::Json::parse(std::string{ msgBody }, root, error))
        {
            LMS_LOG_LASTFM(ERROR, "Cannot parse track.scrobble response: " << error.what());
            return {};
        }

        try
        {
            const Wt::Json::Object& scrobbles{ static_cast<const Wt::Json::Object&>(root.get("scrobbles")) };

            std::vector<ScrobbleResult> res;
            const Wt::Json::Type scrobbleType{ scrobbles.type("scrobble") };
            if (scrobbleType == Wt::Json::Type::Array)
            {
                for (const Wt::Json::Value& item : static_cast<const Wt::Json::Array&>(scrobbles.get("scrobble")))
                    res.push_back(parseScrobbleResult(static_cast<const Wt::Json::Object&>(item)));
            }
            else if (scrobbleType == Wt::Json::Type::Object)
            {
                res.push_back(parseScrobbleResult(static_cast<const Wt::Json::Object&>(scrobbles.get("scrobble"))));
            }
            else
            {
                LMS_LOG_LASTFM(ERROR, "Unexpected 'scrobble' field in track.scrobble response");
                return {};
            }

            if (res.size() != expectedCount)
            {
                LMS_LOG_LASTFM(ERROR, "track.scrobble response has " << res.size() << " scrobble(s), expected " << expectedCount);
                return {};
            }

            return res;
        }
        catch (const Wt::WException& e)
        {
            LMS_LOG_LASTFM(ERROR, "Cannot parse track.scrobble response: " << e.what());
            return {};
        }
    }
} // namespace lms::scrobbling::lastFm::utils
