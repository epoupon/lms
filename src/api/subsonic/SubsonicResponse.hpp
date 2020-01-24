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

#include <map>
#include <string>
#include <vector>


namespace API::Subsonic
{

#define API_VERSION_MAJOR	1
#define API_VERSION_MINOR	12
#define API_VERSION_PATCH	0
#define API_VERSION_STR		"1.12.0"

enum class ResponseFormat
{
	xml,
	json,
};

std::string ResponseFormatToMimeType(ResponseFormat format);

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
			UserNotAuthorized = 50,
			RequestedDataNotFound = 70,
		};

		Error(Code code) : _code {code} {}

		virtual std::string getMessage() const = 0;

		Code getCode() const { return _code; }

	private:
		const Code _code;
};

class GenericError : public Error
{
	public:
		GenericError() : Error {Code::Generic} {}
};

class RequiredParameterMissingError : public Error
{
	public:
		RequiredParameterMissingError() : Error {Code::RequiredParameterMissing} {}
	private:
		std::string getMessage() const override { return "Required parameter is missing."; }
};

class ClientMustUpgradeError : public Error
{
	public:
		ClientMustUpgradeError() : Error {Code::ClientMustUpgrade} {}
	private:
		std::string getMessage() const override { return "Incompatible Subsonic REST protocol version. Client must upgrade."; }
};

class ServerMustUpgradeError : public Error
{
	public:
		ServerMustUpgradeError() : Error {Code::ServerMustUpgrade} {}
	private:
		std::string getMessage() const override { return "Incompatible Subsonic REST protocol version. Server must upgrade."; }
};

class WrongUsernameOrPasswordError : public Error
{
	public:
		WrongUsernameOrPasswordError() : Error {Code::WrongUsernameOrPassword} {}
	private:
		std::string getMessage() const override { return "Wrong username or password."; }
};

class UserNotAuthorizedError : public Error
{
	public:
		UserNotAuthorizedError () : Error {Code::UserNotAuthorized} {}
	private:
		std::string getMessage() const override { return "User is not authorized for the given operation."; }
};

class RequestedDataNotFoundError : public Error
{
	public:
		RequestedDataNotFoundError() : Error {Code::RequestedDataNotFound} {}
	private:
		std::string getMessage() const override { return "The requested data was not found."; }
};

class InternalErrorGenericError : public GenericError
{
	public:
		InternalErrorGenericError(const std::string& message) : _message {message} {}
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

class UserAlreadyExistsGenericError : public GenericError
{
	std::string getMessage() const override { return "User already exists"; }
};

class BadParameterGenericError : public GenericError
{
	public:
		BadParameterGenericError(const std::string& parameterName) : _parameterName {parameterName} {}

	private:
		std::string getMessage() const override { return "Parameter '" + _parameterName + "': bad value"; }

		const std::string _parameterName;
};

class BadParameterFormatGenericError : public GenericError
{
	public:
		BadParameterFormatGenericError(const std::string& parameterName) : _parameterName {parameterName} {}

	private:
		std::string getMessage() const override { return "Parameter '" + _parameterName + "': bad format"; }

		const std::string _parameterName;
};

class Response
{
	public:
		class Node
		{
			public:
				void setAttribute(const std::string& key, const std::string& value);

				// A Node has either a value or some children
				void setValue(const std::string& value);
				Node& createChild(const std::string& key);
				Node& createArrayChild(const std::string& key);

				void addChild(const std::string& key, Node node);
				void addArrayChild(const std::string& key, Node node);

			private:
				friend class Response;
				std::map<std::string, std::string> _attributes;
				std::string _value;
				std::map<std::string, std::vector<Node>> _children;
				std::map<std::string, std::vector<Node>> _childrenArrays;
		};

		static Response createOkResponse();
		static Response createFailedResponse(const Error& error);

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

		void writeJSON(std::ostream& os);
		void writeXML(std::ostream& os);

		Response() = default;
		Node _root;
};

} // namespace

