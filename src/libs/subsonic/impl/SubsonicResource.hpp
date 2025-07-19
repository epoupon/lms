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
#include <unordered_set>

#include <Wt/Http/Response.h>
#include <Wt/WResource.h>

#include "database/objects/UserId.hpp"

#include "RequestContext.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::api::subsonic
{
    class SubsonicResource final : public Wt::WResource
    {
    public:
        SubsonicResource(db::IDb& db);

    private:
        void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;
        ProtocolVersion getServerProtocolVersion(const std::string& clientName) const;

        static void checkProtocolVersion(ProtocolVersion client, ProtocolVersion server);
        RequestContext buildRequestContext(const Wt::Http::Request& request);
        db::UserId authenticateUser(const Wt::Http::Request& request);

        const std::unordered_map<std::string, ProtocolVersion> _serverProtocolVersionsByClient;
        const std::unordered_set<std::string> _openSubsonicDisabledClients;
        const bool _supportUserPasswordAuthentication;

        db::IDb& _db;
    };
} // namespace lms::api::subsonic
