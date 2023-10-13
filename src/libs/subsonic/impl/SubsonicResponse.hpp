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

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "RequestContext.hpp"

namespace API::Subsonic
{

    enum class ResponseFormat
    {
        xml,
        json,
    };

    std::string_view ResponseFormatToMimeType(ResponseFormat format);

    class Error
    {
    public:
        enum class Code
        {
            Generic = 0,
            RequiredParameterMissing = 10,
            ClientMustUpgrade = 20,
            ServerMustUpgrade = 30,
            WrongUsernameOrPassword = 40,
            TokenAuthenticationNotSupportedForLDAPUsers = 41,
            UserNotAuthorized = 50,
            RequestedDataNotFound = 70,
        };

        Error(Code code) : _code{ code } {}

        virtual std::string getMessage() const = 0;

        Code getCode() const { return _code; }

    private:
        const Code _code;
    };

    class GenericError : public Error
    {
    public:
        GenericError() : Error{ Code::Generic } {}
    };

    class RequiredParameterMissingError : public Error
    {
    public:
        RequiredParameterMissingError(std::string_view param)
            : Error{ Code::RequiredParameterMissing }
            , _param{ param }
        {}

    private:
        std::string getMessage() const override { return "Required parameter '" + _param + "' is missing."; }
        std::string _param;
    };

    class ClientMustUpgradeError : public Error
    {
    public:
        ClientMustUpgradeError() : Error{ Code::ClientMustUpgrade } {}
    private:
        std::string getMessage() const override { return "Incompatible Subsonic REST protocol version. Client must upgrade."; }
    };

    class ServerMustUpgradeError : public Error
    {
    public:
        ServerMustUpgradeError() : Error{ Code::ServerMustUpgrade } {}
    private:
        std::string getMessage() const override { return "Incompatible Subsonic REST protocol version. Server must upgrade."; }
    };

    class WrongUsernameOrPasswordError : public Error
    {
    public:
        WrongUsernameOrPasswordError() : Error{ Code::WrongUsernameOrPassword } {}
    private:
        std::string getMessage() const override { return "Wrong username or password."; }
    };

    class TokenAuthenticationNotSupportedForLDAPUsersError : public Error
    {
    public:
        TokenAuthenticationNotSupportedForLDAPUsersError() : Error{ Code::TokenAuthenticationNotSupportedForLDAPUsers } {}
    private:
        std::string getMessage() const override { return "Token authentication not supported for LDAP users."; }
    };

    class UserNotAuthorizedError : public Error
    {
    public:
        UserNotAuthorizedError() : Error{ Code::UserNotAuthorized } {}
    private:
        std::string getMessage() const override { return "User is not authorized for the given operation."; }
    };

    class RequestedDataNotFoundError : public Error
    {
    public:
        RequestedDataNotFoundError() : Error{ Code::RequestedDataNotFound } {}
    private:
        std::string getMessage() const override { return "The requested data was not found."; }
    };

    class InternalErrorGenericError : public GenericError
    {
    public:
        InternalErrorGenericError(const std::string& message) : _message{ message } {}
    private:
        std::string getMessage() const override { return "Internal error: " + _message; }
        const std::string _message;
    };

    class LoginThrottledGenericError : public GenericError
    {
        std::string getMessage() const override { return "Login throttled, too many attempts"; }
    };

    class NotImplementedGenericError : public GenericError
    {
        std::string getMessage() const override { return "Not implemented"; }
    };

    class UnknownEntryPointGenericError : public GenericError
    {
        std::string getMessage() const override { return "Unknown API method"; }
    };

    class PasswordTooWeakGenericError : public GenericError
    {
        std::string getMessage() const override { return "Password too weak"; }
    };

    class PasswordMustMatchLoginNameGenericError : public GenericError
    {
        std::string getMessage() const override { return "Password must match login name"; }
    };

    class DemoUserCannotChangePasswordGenericError : public GenericError
    {
        std::string getMessage() const override { return "Demo user cannot change its password"; }
    };

    class UserAlreadyExistsGenericError : public GenericError
    {
        std::string getMessage() const override { return "User already exists"; }
    };

    class BadParameterGenericError : public GenericError
    {
    public:
        BadParameterGenericError(const std::string& parameterName) : _parameterName{ parameterName } {}

    private:
        std::string getMessage() const override { return "Parameter '" + _parameterName + "': bad value"; }

        const std::string _parameterName;
    };

    class Response
    {
    public:
        class Node
        {
        public:
            void setAttribute(std::string_view key, std::string_view value);

            template <typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
            void setAttribute(std::string_view key, T value)
            {
                if constexpr (std::is_same<bool, T>::value)
                    _attributes[std::string{ key }] = value;
                else if constexpr (std::is_floating_point<T>::value)
                    _attributes[std::string{ key }] = static_cast<float>(value);
                else if constexpr (std::is_integral<T>::value)
                    _attributes[std::string{ key }] = static_cast<long long>(value);
                else
                    static_assert("Unhandled type");
            }

            // A Node has either a single value or an array of values or some children
            void setValue(std::string_view value);
            void setValue(long long value);
            Node& createChild(const std::string& key);
            Node& createArrayChild(const std::string& key);

            void addChild(const std::string& key, Node node);
            void createEmptyArrayChild(std::string_view key);
            void addArrayChild(std::string_view key, Node node);
            void createEmptyArrayValue(std::string_view key);
            void addArrayValue(std::string_view key, std::string_view value);
            void addArrayValue(std::string_view key, long long value);

        private:
            void setVersionAttribute(ProtocolVersion version);

            friend class Response;
            using ValueType = std::variant<std::string, bool, float, long long>;
            std::map<std::string, ValueType> _attributes;
            std::optional<ValueType> _value;
            std::map<std::string, std::vector<Node>> _children;
            std::map<std::string, std::vector<Node>> _childrenArrays;

            using ValuesType = std::vector<ValueType>;
            std::map<std::string, ValuesType> _childrenValues;
        };

        static Response createOkResponse(ProtocolVersion protocolVersion);
        static Response createFailedResponse(ProtocolVersion protocolVersion, const Error& error);

        virtual ~Response() {}
        Response(const Response&) = delete;
        Response& operator=(const Response&) = delete;
        Response(Response&&) = default;
        Response& operator=(Response&&) = default;

        void addNode(const std::string& key, Node node);
        Node& createNode(const std::string& key);
        Node& createArrayNode(const std::string& key);

        void write(std::ostream& os, ResponseFormat format);

    private:
        static Response createResponseCommon(ProtocolVersion protocolVersion, const Error* error = nullptr);
        void writeJSON(std::ostream& os);
        void writeXML(std::ostream& os);

        Response() = default;
        Node _root;
    };

} // namespace

