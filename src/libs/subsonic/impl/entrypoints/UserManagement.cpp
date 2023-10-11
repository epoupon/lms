#include "UserManagement.hpp"

#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "services/auth/IPasswordService.hpp"
#include "utils/Service.hpp"
#include "responses/User.hpp"
#include "ParameterParsing.hpp"
#include "Utils.hpp"

namespace API::Subsonic
{
    using namespace Database;

    namespace {
        void checkUserIsMySelfOrAdmin(RequestContext& context, const std::string& username)
        {
            User::pointer currentUser{ User::find(context.dbSession, context.userId) };
            if (!currentUser)
                throw RequestedDataNotFoundError{};

            if (currentUser->getLoginName() != username && !currentUser->isAdmin())
                throw UserNotAuthorizedError{};
        }
    }

    Response handleGetUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };

        auto transaction{ context.dbSession.createSharedTransaction() };

        checkUserIsMySelfOrAdmin(context, username);

        const User::pointer user{ User::find(context.dbSession, username) };
        if (!user)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("user", createUserNode(user));

        return response;
    }

    Response handleGetUsersRequest(RequestContext& context)
    {
        auto transaction{ context.dbSession.createSharedTransaction() };

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& usersNode{ response.createNode("users") };

        const auto userIds{ User::find(context.dbSession, User::FindParameters {}) };
        for (const UserId userId : userIds.results)
        {
            const User::pointer user{ User::find(context.dbSession, userId) };
            usersNode.addArrayChild("user", createUserNode(user));
        }

        return response;
    }

    Response handleCreateUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };
        std::string password{ decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(context.parameters, "password")) };
        // Just ignore all the other fields as we don't handle them

        Database::UserId userId;
        {
            auto transaction{ context.dbSession.createUniqueTransaction() };

            User::pointer user{ User::find(context.dbSession, username) };
            if (user)
                throw UserAlreadyExistsGenericError{};

            user = context.dbSession.create<User>(username);
            userId = user->getId();
        }

        auto removeCreatedUser{ [&]()
        {
            auto transaction {context.dbSession.createUniqueTransaction()};
            User::pointer user {User::find(context.dbSession, userId)};
            if (user)
                user.remove();
        } };

        try
        {
            Service<Auth::IPasswordService>::get()->setPassword(userId, password);
        }
        catch (const Auth::PasswordMustMatchLoginNameException&)
        {
            removeCreatedUser();
            throw PasswordMustMatchLoginNameGenericError{};
        }
        catch (const Auth::PasswordTooWeakException&)
        {
            removeCreatedUser();
            throw PasswordTooWeakGenericError{};
        }
        catch (const Auth::Exception& exception)
        {
            removeCreatedUser();
            throw UserNotAuthorizedError{};
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleDeleteUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };

        auto transaction{ context.dbSession.createUniqueTransaction() };

        User::pointer user{ User::find(context.dbSession, username) };
        if (!user)
            throw RequestedDataNotFoundError{};

        // cannot delete ourself
        if (user->getId() == context.userId)
            throw UserNotAuthorizedError{};

        user.remove();

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleUpdateUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };
        std::optional<std::string> password{ getParameterAs<std::string>(context.parameters, "password") };

        UserId userId;
        {
            auto transaction{ context.dbSession.createSharedTransaction() };

            User::pointer user{ User::find(context.dbSession, username) };
            if (!user)
                throw RequestedDataNotFoundError{};

            userId = user->getId();
        }

        if (password)
        {
            Utils::checkSetPasswordImplemented();

            try
            {
                Service<::Auth::IPasswordService>()->setPassword(userId, decodePasswordIfNeeded(*password));
            }
            catch (const Auth::PasswordMustMatchLoginNameException&)
            {
                throw PasswordMustMatchLoginNameGenericError{};
            }
            catch (const Auth::PasswordTooWeakException&)
            {
                throw PasswordTooWeakGenericError{};
            }
            catch (const Auth::Exception&)
            {
                throw UserNotAuthorizedError{};
            }
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleChangePassword(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };
        std::string password{ decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(context.parameters, "password")) };

        try
        {
            Database::UserId userId;
            {
                auto transaction{ context.dbSession.createSharedTransaction() };

                checkUserIsMySelfOrAdmin(context, username);

                User::pointer user{ User::find(context.dbSession, username) };
                if (!user)
                    throw UserNotAuthorizedError{};

                userId = user->getId();
            }

            Service<Auth::IPasswordService>::get()->setPassword(userId, password);
        }
        catch (const Auth::PasswordMustMatchLoginNameException&)
        {
            throw PasswordMustMatchLoginNameGenericError{};
        }
        catch (const Auth::PasswordTooWeakException&)
        {
            throw PasswordTooWeakGenericError{};
        }
        catch (const Auth::Exception& authException)
        {
            throw UserNotAuthorizedError{};
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }
}