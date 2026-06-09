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

#include "LastFmBackend.hpp"

#include <map>

#include "core/IConfig.hpp"
#include "core/Service.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"

namespace lms::scrobbling::lastFm
{
    namespace
    {
        bool canBeScrobbled(db::Session& session, db::TrackId trackId, std::chrono::seconds playedDuration)
        {
            auto transaction{ session.createReadTransaction() };

            const db::Track::pointer track{ db::Track::find(session, trackId) };
            if (!track)
                return false;

            const bool res{ track->getDuration() >= std::chrono::seconds{ 30 } && (playedDuration >= std::chrono::minutes{ 4 } || playedDuration >= track->getDuration() / 2) };
            if (!res)
                LOG(DEBUG, "Track cannot be scrobbled: played duration too short (" << playedDuration.count() << "s, total = " << std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count() << "s)");

            return res;
        }
    } // namespace

    LastFmBackend::LastFmBackend(boost::asio::io_context& ioContext, db::IDb& db)
        : _db{ db }
        , _authBaseUrl{ core::Service<core::IConfig>::get()->getString("lastfm-auth-base-url", "https://www.last.fm") }
        , _client{ core::http::createClient(ioContext, core::Service<core::IConfig>::get()->getString("lastfm-api-base-url", "https://ws.audioscrobbler.com")) }
        , _synchronizer{ ioContext, db, *_client }
    {
        LOG(INFO, "Starting Last.fm backend");
    }

    LastFmBackend::~LastFmBackend()
    {
        LOG(INFO, "Stopped Last.fm backend");
    }

    void LastFmBackend::listenStarted(const Listen& listen)
    {
        _synchronizer.enqueListenNow(listen);
    }

    void LastFmBackend::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> playedDuration)
    {
        if (playedDuration && !canBeScrobbled(_db.getTLSSession(), listen.trackId, *playedDuration))
            return;

        const TimedListen timedListen{ listen, Wt::WDateTime::currentDateTime() };
        _synchronizer.enqueListen(timedListen);
    }

    void LastFmBackend::addTimedListen(const TimedListen& timedListen)
    {
        _synchronizer.enqueListen(timedListen);
    }

    void LastFmBackend::initiateLastFmLink(db::UserId userId,
                                           std::string_view apiKey,
                                           std::string_view apiSecret,
                                           std::function<void(std::string_view authUrl)> onSuccess,
                                           std::function<void()> onFailure)
    {
        const std::string apiKeyStr{ apiKey };
        const std::string apiSecretStr{ apiSecret };

        const std::map<std::string, std::string> params{
            { "api_key", apiKeyStr },
            { "format", "json" },
            { "method", "auth.getToken" },
        };
        const std::string sig{ utils::computeApiSig(params, apiSecretStr) };

        core::http::ClientGETRequestParameters request;
        request.relativeUrl = "/2.0/?method=auth.getToken&api_key=" + apiKeyStr + "&api_sig=" + sig + "&format=json";
        request.onSuccessFunc = [this, userId, apiKeyStr, apiSecretStr, onSuccess = std::move(onSuccess), onFailure](const Wt::Http::Message& msg) {
            const std::string token{ utils::parseAuthToken(msg.body()) };
            if (token.empty())
            {
                LOG(WARNING, "auth.getToken: failed to parse token");
                onFailure();
                return;
            }

            {
                std::scoped_lock lock{ _pendingAuthsMutex };
                _pendingAuths[userId] = PendingAuth{ .apiKey = apiKeyStr, .apiSecret = apiSecretStr, .token = token };
            }

            const std::string authUrl{ _authBaseUrl + "/api/auth/?api_key=" + apiKeyStr + "&token=" + token };
            onSuccess(authUrl);
        };
        request.onFailureFunc = [onFailure = std::move(onFailure)] {
            LOG(WARNING, "auth.getToken: HTTP request failed");
            onFailure();
        };
        _client->sendGETRequest(std::move(request));
    }

    void LastFmBackend::continueLastFmLink(db::UserId userId,
                                           std::function<void()> onSuccess,
                                           std::function<void()> onFailure)
    {
        PendingAuth pending;
        {
            std::scoped_lock lock{ _pendingAuthsMutex };
            auto it{ _pendingAuths.find(userId) };
            if (it == _pendingAuths.end())
            {
                LOG(WARNING, "continueLastFmLink: no pending auth for user");
                onFailure();
                return;
            }
            pending = it->second;
        }

        const std::map<std::string, std::string> params{
            { "api_key", pending.apiKey },
            { "format", "json" },
            { "method", "auth.getSession" },
            { "token", pending.token },
        };
        const std::string sig{ utils::computeApiSig(params, pending.apiSecret) };

        core::http::ClientGETRequestParameters request;
        request.relativeUrl = "/2.0/?method=auth.getSession&api_key=" + pending.apiKey + "&token=" + pending.token + "&api_sig=" + sig + "&format=json";
        request.onSuccessFunc = [this, userId, pending, onSuccess = std::move(onSuccess), onFailure](const Wt::Http::Message& msg) {
            const std::string sessionKey{ utils::parseSessionKey(msg.body()) };
            if (sessionKey.empty())
            {
                LOG(WARNING, "auth.getSession: failed to parse session key");
                onFailure();
                return;
            }

            {
                db::Session& session{ _db.getTLSSession() };
                auto transaction{ session.createWriteTransaction() };
                if (db::User::pointer user{ db::User::find(session, userId) })
                {
                    user.modify()->setLastFmApiKey(pending.apiKey);
                    user.modify()->setLastFmApiSecret(pending.apiSecret);
                    user.modify()->setLastFmSessionKey(sessionKey);
                }
            }

            {
                std::scoped_lock lock{ _pendingAuthsMutex };
                _pendingAuths.erase(userId);
            }

            LOG(INFO, "Last.fm account linked for user " << userId.toString());
            onSuccess();
        };
        request.onFailureFunc = [onFailure = std::move(onFailure)] {
            LOG(WARNING, "auth.getSession: HTTP request failed");
            onFailure();
        };

        _client->sendGETRequest(std::move(request));
    }
} // namespace lms::scrobbling::lastFm
