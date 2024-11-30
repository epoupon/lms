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

#include "core/LiteralString.hpp"

#include "ProtocolVersion.hpp"
#include "ResponseFormat.hpp"
#include "SubsonicResponseAllocator.hpp"

namespace lms::api::subsonic
{
    // Max count expected from all API methods that expose a count
    static inline constexpr std::size_t defaultMaxCountSize{ 1'000 };

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
            ProvidedAuthenticationMechanismNotSupported = 42,
            MultipleConflictingAuthenticationMechanismsProvided = 43,
            InvalidAPIkey = 44,
            UserNotAuthorized = 50,
            RequestedDataNotFound = 70,
        };

        Error(Code code)
            : _code{ code } {}

        virtual std::string getMessage() const = 0;

        Code getCode() const { return _code; }

    private:
        const Code _code;
    };

    class GenericError : public Error
    {
    public:
        GenericError()
            : Error{ Code::Generic } {}
    };

    class RequiredParameterMissingError : public Error
    {
    public:
        RequiredParameterMissingError(std::string_view param)
            : Error{ Code::RequiredParameterMissing }
            , _param{ param }
        {
        }

    private:
        std::string getMessage() const override { return "Required parameter '" + _param + "' is missing."; }
        std::string _param;
    };

    class ClientMustUpgradeError : public Error
    {
    public:
        ClientMustUpgradeError()
            : Error{ Code::ClientMustUpgrade } {}

    private:
        std::string getMessage() const override { return "Incompatible Subsonic REST protocol version. Client must upgrade."; }
    };

    class ServerMustUpgradeError : public Error
    {
    public:
        ServerMustUpgradeError()
            : Error{ Code::ServerMustUpgrade } {}

    private:
        std::string getMessage() const override { return "Incompatible Subsonic REST protocol version. Server must upgrade."; }
    };

    class WrongUsernameOrPasswordError : public Error
    {
    public:
        WrongUsernameOrPasswordError()
            : Error{ Code::WrongUsernameOrPassword } {}

    private:
        std::string getMessage() const override { return "Wrong username or password."; }
    };

    class TokenAuthenticationNotSupportedForLDAPUsersError : public Error
    {
    public:
        TokenAuthenticationNotSupportedForLDAPUsersError()
            : Error{ Code::TokenAuthenticationNotSupportedForLDAPUsers } {}

    private:
        std::string getMessage() const override { return "Token authentication not supported for LDAP users."; }
    };

    class ProvidedAuthenticationMechanismNotSupportedError : public Error
    {
    public:
        ProvidedAuthenticationMechanismNotSupportedError()
            : Error{ Code::ProvidedAuthenticationMechanismNotSupported } {}

    private:
        std::string getMessage() const override
        {
            return "Provided authentication mechanism not supported.";
        }
    };

    class MultipleConflictingAuthenticationMechanismsProvidedError : public Error
    {
    public:
        MultipleConflictingAuthenticationMechanismsProvidedError()
            : Error{ Code::MultipleConflictingAuthenticationMechanismsProvided } {}

    private:
        std::string getMessage() const override
        {
            return "Multiple conflicting authentication mechanisms provided.";
        }
    };

    class InvalidAPIkeyError : public Error
    {
    public:
        InvalidAPIkeyError()
            : Error{ Code::InvalidAPIkey } {}

    private:
        std::string getMessage() const override
        {
            return "Invalid API key.";
        }
    };

    class UserNotAuthorizedError : public Error
    {
    public:
        UserNotAuthorizedError()
            : Error{ Code::UserNotAuthorized } {}

    private:
        std::string getMessage() const override { return "User is not authorized for the given operation."; }
    };

    class RequestedDataNotFoundError : public Error
    {
    public:
        RequestedDataNotFoundError()
            : Error{ Code::RequestedDataNotFound } {}

    private:
        std::string getMessage() const override { return "The requested data was not found."; }
    };

    class InternalErrorGenericError : public GenericError
    {
    public:
        InternalErrorGenericError(std::string_view message)
            : _message{ message } {}

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

    class BadParameterGenericError : public GenericError
    {
    public:
        BadParameterGenericError(const std::string& parameterName)
            : _parameterName{ parameterName } {}

    private:
        std::string getMessage() const override { return "Parameter '" + _parameterName + "': bad value"; }

        const std::string _parameterName;
    };

    class ParameterValueTooHighGenericError : public GenericError
    {
    public:
        ParameterValueTooHighGenericError(std::string_view parameterName, std::size_t max)
            : _parameterName{ parameterName }
            , _max{ max } {}

    private:
        std::string getMessage() const override { return "Parameter '" + _parameterName + "': bad value (max is " + std::to_string(_max) + ")"; }

        const std::string _parameterName;
        std::size_t _max;
    };

    class Response final
    {
    public:
        class Node
        {
        public:
            using Key = core::LiteralString;

            void setAttribute(Key key, std::string_view value);

            template<typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
            void setAttribute(Key key, T value)
            {
                if constexpr (std::is_same<bool, T>::value)
                    _attributes[key] = value;
                else if constexpr (std::is_floating_point<T>::value)
                    _attributes[key] = static_cast<float>(value);
                else if constexpr (std::is_integral<T>::value)
                    _attributes[key] = static_cast<long long>(value);
                else
                    static_assert("Unhandled type");
            }

            // A Node has either a single value or an array of values or some children
            void setValue(std::string_view value);
            void setValue(long long value);
            Node& createChild(Key key);
            Node& createArrayChild(Key key);

            void addChild(Key key, Node&& node);
            void createEmptyArrayChild(Key key);
            void addArrayChild(Key key, Node&& node);
            void createEmptyArrayValue(Key key);
            void addArrayValue(Key key, std::string_view value);
            void addArrayValue(Key key, long long value);

        private:
            void setVersionAttribute(ProtocolVersion version);

            friend class Response;

            template<typename Key, typename Value>
            using map = std::map<Key, Value, std::less<Key>, ResponseAllocator<std::pair<const Key, Value>>>;

            template<typename T>
            using vector = std::vector<T, ResponseAllocator<T>>;

            using string = std::basic_string<char, std::char_traits<char>, ResponseAllocator<char>>;

            using ValueType = std::variant<string, bool, float, long long>;
            map<Key, ValueType> _attributes;
            std::optional<ValueType> _value;
            map<Key, Node> _children;
            map<Key, vector<Node>> _childrenArrays;

            using ValuesType = vector<ValueType>;
            map<Key, ValuesType> _childrenValues;
        };

        static Response createOkResponse(ProtocolVersion protocolVersion);
        static Response createFailedResponse(ProtocolVersion protocolVersion, const Error& error);

        ~Response() = default;
        Response(const Response&) = delete;
        Response& operator=(const Response&) = delete;
        Response(Response&&) = default;
        Response& operator=(Response&&) = default;

        void addNode(Node::Key key, Node&& node);
        Node& createNode(Node::Key key);
        Node& createArrayNode(Node::Key key);

        void write(std::ostream& os, ResponseFormat format) const;

    private:
        static Response createResponseCommon(ProtocolVersion protocolVersion, const Error* error = nullptr);

        class JsonSerializer
        {
        public:
            void serializeNode(std::ostream& os, const Node& node);
            void serializeValue(std::ostream& os, const Node::ValueType& value);
            void serializeEscapedString(std::ostream&, std::string_view str);
        };

        void writeJSON(std::ostream& os) const;
        void writeXML(std::ostream& os) const;

        Response() = default;
        Node _root;
    };

} // namespace lms::api::subsonic
