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
#include <optional>
#include <string>
#include <string_view>

#include <Wt/Http/Request.h>

#include "database/objects/User.hpp"

#include "ProtocolVersion.hpp"
#include "ResponseFormat.hpp"
#include "SubsonicResourceConfig.hpp"

namespace lms::db
{
    class ISession;
} // namespace lms::db

namespace lms::api::subsonic
{
    class RequestContext
    {
    public:
        RequestContext(const Wt::Http::Request& request, db::ISession& dbSession, const SubsonicResourceConfig& config);
        ~RequestContext();
        RequestContext(const RequestContext&) = delete;
        RequestContext& operator=(const RequestContext&) = delete;

        using ParameterMap = Wt::Http::ParameterMap;

        const ParameterMap& getParameters() const;
        std::istream& getBody() const;

        db::ISession& getDbSession();

        void setUser(const std::optional<db::User>& user);
        const std::optional<db::User>& getUser() const { return _user; }

        std::string getClientIpAddr() const;
        std::string_view getClientName() const;

        ResponseFormat getResponseFormat() const;
        bool isOpenSubsonicEnabled() const;

    private:
        const Wt::Http::Request& _request;
        db::ISession& _dbSession;
        std::optional<db::User> _user;
        const SubsonicResourceConfig& _config;

        const std::string _clientName;
        const ProtocolVersion _clientProtocolVersion;
        const ResponseFormat _responseFormat;

        const bool _isOpenSubsonicEnabled;
    };
} // namespace lms::api::subsonic
