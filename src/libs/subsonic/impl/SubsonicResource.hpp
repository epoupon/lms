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

#include <Wt/Http/Request.h>
#include <Wt/Http/Response.h>
#include <Wt/WResource.h>

#include "database/objects/UserId.hpp"

#include "SubsonicResourceConfig.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::api::subsonic
{
    class RequestContext;

    class SubsonicResource final : public Wt::WResource
    {
    public:
        SubsonicResource(db::IDb& db);

    private:
        void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;

        using MediaRetrievalHandlerFunc = std::function<void(RequestContext&, const Wt::Http::Request&, Wt::Http::Response&)>;
        void handleMediaRetrievalRequest(MediaRetrievalHandlerFunc handler, const Wt::Http::Request& request, Wt::Http::Response& response);

        db::UserId authenticateUser(const Wt::Http::Request& request);

        const SubsonicResourceConfig _config;
        db::IDb& _db;
    };
} // namespace lms::api::subsonic
