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

#include "RequestContext.hpp"

#include "ParameterParsing.hpp"
#include "SubsonicResourceConfig.hpp"
#include "SubsonicResponse.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        void checkProtocolVersion(ProtocolVersion client, ProtocolVersion server)
        {
            if (client.major > server.major)
                throw ServerMustUpgradeError{};
            if (client.major < server.major)
                throw ClientMustUpgradeError{};
            if (client.minor > server.minor)
                throw ServerMustUpgradeError{};
            if (client.minor == server.minor)
            {
                if (client.patch > server.patch)
                    throw ServerMustUpgradeError{};
            }
        }
    } // namespace

    RequestContext::RequestContext(const Wt::Http::Request& request, db::Session& dbSession, db::ObjectPtr<db::User> user, const SubsonicResourceConfig& config)
        : _request{ request }
        , _dbSession{ dbSession }
        , _user{ user }
        , _config{ config }
        , _clientName{ getMandatoryParameterAs<std::string>(_request.getParameterMap(), "c") }
        , _clientProtocolVersion{ getMandatoryParameterAs<ProtocolVersion>(_request.getParameterMap(), "v") }
        , _responseFormat{ getParameterAs<std::string>(request.getParameterMap(), "f").value_or("xml") == "json" ? ResponseFormat::json : ResponseFormat::xml }
        , _serverProtocolVersion{ _config.serverProtocolVersionsByClient.contains(_clientName) ? _config.serverProtocolVersionsByClient.at(_clientName) : defaultServerProtocolVersion }
        , _isOpenSubsonicEnabled{ !_config.openSubsonicDisabledClients.contains(_clientName) }
    {
        checkProtocolVersion(_clientProtocolVersion, _serverProtocolVersion);
    }

    RequestContext::~RequestContext() = default;

    const RequestContext::ParameterMap& RequestContext::getParameters() const
    {
        return _request.getParameterMap();
    }

    std::istream& RequestContext::getBody() const
    {
        return _request.in();
    }

    db::Session& RequestContext::getDbSession()
    {
        return _dbSession;
    }

    db::ObjectPtr<db::User> RequestContext::getUser() const
    {
        return _user;
    }

    std::string RequestContext::getClientIpAddr() const
    {
        return _request.clientAddress();
    }

    std::string_view RequestContext::getClientName() const
    {
        return _clientName;
    }

    ProtocolVersion RequestContext::getServerProtocolVersion() const
    {
        return _serverProtocolVersion;
    }

    ResponseFormat RequestContext::getResponseFormat() const
    {
        return _responseFormat;
    }

    bool RequestContext::isOpenSubsonicEnabled() const
    {
        return _isOpenSubsonicEnabled;
    }

} // namespace lms::api::subsonic
