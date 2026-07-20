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

#include "UserManagement.hpp"

#include "database/Session.hpp"
#include "database/objects/User.hpp"

#include "ParameterParsing.hpp"
#include "responses/User.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        void checkUserIsMySelfOrAdmin(RequestContext& context, const std::string& username)
        {
            if (context.getUser()->getLoginName() != username && !context.getUser()->isAdmin())
                throw UserNotAuthorizedError{};
        }
    } // namespace

    Response handleGetUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.getParameters(), "username") };

        auto transaction{ context.getDbSession().createReadTransaction() };

        checkUserIsMySelfOrAdmin(context, username);

        const User::pointer user{ User::find(context.getDbSession(), username) };
        if (!user)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse() };
        response.addNode("user", createUserNode(context, user));

        return response;
    }

    Response handleGetUsersRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse() };
        Response::Node& usersNode{ response.createNode("users") };

        auto transaction{ context.getDbSession().createReadTransaction() };
        User::find(context.getDbSession(), User::FindParameters{}, [&](const User::pointer& user) {
            usersNode.addArrayChild("user", createUserNode(context, user));
        });

        return response;
    }
} // namespace lms::api::subsonic