#include "UserManagement.hpp"

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"
#include "services/auth/IPasswordService.hpp"

#include "ParameterParsing.hpp"
#include "Utils.hpp"
#include "responses/User.hpp"

namespace lms::api::subsonic
{
    using namespace db;

    namespace
    {
        void checkUserIsMySelfOrAdmin(RequestContext& context, const std::string& username)
        {
            if (context.user->getLoginName() != username && !context.user->isAdmin())
                throw UserNotAuthorizedError{};
        }
    } // namespace

    Response handleGetUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };

        auto transaction{ context.dbSession.createReadTransaction() };

        checkUserIsMySelfOrAdmin(context, username);

        const User::pointer user{ User::find(context.dbSession, username) };
        if (!user)
            throw RequestedDataNotFoundError{};

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("user", createUserNode(context, user));

        return response;
    }

    Response handleGetUsersRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        Response::Node& usersNode{ response.createNode("users") };

        auto transaction{ context.dbSession.createReadTransaction() };
        User::find(context.dbSession, User::FindParameters{}, [&](const User::pointer& user) {
            usersNode.addArrayChild("user", createUserNode(context, user));
        });

        return response;
    }

    Response handleCreateUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };
        std::string password{ decodePasswordIfNeeded(getMandatoryParameterAs<std::string>(context.parameters, "password")) };
        // Just ignore all the other fields as we don't handle them

        db::UserId userId;
        {
            auto transaction{ context.dbSession.createWriteTransaction() };

            User::pointer user{ User::find(context.dbSession, username) };
            if (user)
                throw UserAlreadyExistsGenericError{};

            user = context.dbSession.create<User>(username);
            userId = user->getId();
        }

        auto removeCreatedUser{ [&] {
            auto transaction{ context.dbSession.createWriteTransaction() };
            User::pointer user{ User::find(context.dbSession, userId) };
            if (user)
                user.remove();
        } };

        try
        {
            core::Service<auth::IPasswordService>::get()->setPassword(userId, password);
        }
        catch (const auth::PasswordMustMatchLoginNameException&)
        {
            removeCreatedUser();
            throw PasswordMustMatchLoginNameGenericError{};
        }
        catch (const auth::PasswordTooWeakException&)
        {
            removeCreatedUser();
            throw PasswordTooWeakGenericError{};
        }
        catch (const auth::Exception& exception)
        {
            removeCreatedUser();
            throw UserNotAuthorizedError{};
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleDeleteUserRequest(RequestContext& context)
    {
        std::string username{ getMandatoryParameterAs<std::string>(context.parameters, "username") };

        auto transaction{ context.dbSession.createWriteTransaction() };

        User::pointer user{ User::find(context.dbSession, username) };
        if (!user)
            throw RequestedDataNotFoundError{};

        // cannot delete ourself
        if (user->getId() == context.user->getId())
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
            auto transaction{ context.dbSession.createReadTransaction() };

            User::pointer user{ User::find(context.dbSession, username) };
            if (!user)
                throw RequestedDataNotFoundError{};

            userId = user->getId();
        }

        if (password)
        {
            utils::checkSetPasswordImplemented();

            try
            {
                core::Service<auth::IPasswordService>()->setPassword(userId, decodePasswordIfNeeded(*password));
            }
            catch (const auth::PasswordMustMatchLoginNameException&)
            {
                throw PasswordMustMatchLoginNameGenericError{};
            }
            catch (const auth::PasswordTooWeakException&)
            {
                throw PasswordTooWeakGenericError{};
            }
            catch (const auth::Exception&)
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
            db::UserId userId;
            {
                auto transaction{ context.dbSession.createReadTransaction() };

                checkUserIsMySelfOrAdmin(context, username);

                User::pointer user{ User::find(context.dbSession, username) };
                if (!user)
                    throw UserNotAuthorizedError{};

                userId = user->getId();
            }

            core::Service<auth::IPasswordService>::get()->setPassword(userId, password);
        }
        catch (const auth::PasswordMustMatchLoginNameException&)
        {
            throw PasswordMustMatchLoginNameGenericError{};
        }
        catch (const auth::PasswordTooWeakException&)
        {
            throw PasswordTooWeakGenericError{};
        }
        catch (const auth::Exception& authException)
        {
            throw UserNotAuthorizedError{};
        }

        return Response::createOkResponse(context.serverProtocolVersion);
    }
} // namespace lms::api::subsonic