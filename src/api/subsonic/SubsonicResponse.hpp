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

		enum class CustomType
		{
			BadId,
			NotImplemented,
			InternalError,
		};

		Error(Code code);
		Error(CustomType customType);

		Code getCode() const { return _code; }
		const std::string& getMessage() const { return _message; }

	private:
		Code _code;
		std::string _message;
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

		void writeJson(std::ostream& os);
		void write(std::ostream& os, ResponseFormat format);
	private:

		Response() = default;
		Node _root;
};

} // namespace

