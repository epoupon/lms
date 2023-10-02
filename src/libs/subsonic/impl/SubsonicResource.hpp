/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <string>
#include <unordered_map>

#include <Wt/WResource.h>
#include <Wt/Http/Response.h>

#include "services/database/Types.hpp"
#include "ClientInfo.hpp"
#include "RequestContext.hpp"

namespace Database
{
    class Db;
}

namespace API::Subsonic
{

    class SubsonicResource final : public Wt::WResource
    {
        public:
            SubsonicResource(Database::Db& db);

        private:
            void handleRequest(const Wt::Http::Request &request, Wt::Http::Response &response) override;
            ProtocolVersion getServerProtocolVersion(const std::string& clientName) const;

            static void checkProtocolVersion(ProtocolVersion client, ProtocolVersion server);
            ClientInfo getClientInfo(const Wt::Http::ParameterMap& parameters);
            RequestContext buildRequestContext(const Wt::Http::Request& request);
            Database::UserId authenticateUser(const Wt::Http::Request& request, const ClientInfo& clientInfo);

            const std::unordered_map<std::string, ProtocolVersion> _serverProtocolVersionsByClient;
            Database::Db& _db;
    };

} // namespace
