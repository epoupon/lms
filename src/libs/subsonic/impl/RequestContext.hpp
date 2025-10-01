/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <iosfwd>
#include <string>
#include <string_view>

#include <Wt/Http/Request.h>

#include "database/Object.hpp"

#include "ProtocolVersion.hpp"
#include "ResponseFormat.hpp"
#include "SubsonicResourceConfig.hpp"

namespace lms::db
{
    class Session;
    class User;
} // namespace lms::db

namespace lms::api::subsonic
{
    class RequestContext
    {
    public:
        RequestContext(const Wt::Http::Request& request, db::Session& dbSession, db::ObjectPtr<db::User> user, const SubsonicResourceConfig& config);
        ~RequestContext();
        RequestContext(const RequestContext&) = delete;
        RequestContext& operator=(const RequestContext&) = delete;

        using ParameterMap = Wt::Http::ParameterMap;

        const ParameterMap& getParameters() const;
        std::istream& getBody() const;

        db::Session& getDbSession();
        db::ObjectPtr<db::User> getUser() const;

        std::string getClientIpAddr() const;
        std::string_view getClientName() const;

        ProtocolVersion getServerProtocolVersion() const;
        ResponseFormat getResponseFormat() const;
        bool isOpenSubsonicEnabled() const;

    private:
        const Wt::Http::Request& _request;
        db::Session& _dbSession;
        db::ObjectPtr<db::User> _user;
        const SubsonicResourceConfig& _config;

        const std::string _clientName;
        const ProtocolVersion _clientProtocolVersion;
        const ResponseFormat _responseFormat;

        const ProtocolVersion _serverProtocolVersion;
        const bool _isOpenSubsonicEnabled;
    };

} // namespace lms::api::subsonic
