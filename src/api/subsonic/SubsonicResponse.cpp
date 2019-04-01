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

#include "SubsonicResponse.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#define API_VERSION	"1.12.0"

namespace API::Subsonic
{

std::string
ResponseFormatToMimeType(ResponseFormat format)
{
	switch (format)
	{
		case ResponseFormat::xml: return "text/xml";
		case ResponseFormat::json: return "application/json";
	}

	return "";
}

static
const char*
ErrorCodeToString(Error::Code error)
{
	switch (error)
	{
		case Error::Code::RequiredParameterMissing:
			return "Required parameter is missing.";
		case Error::Code::ClientMustUpgrade:
			return "Incompatible Subsonic REST protocol version. Client must upgrade.";
		case Error::Code::ServerMustUpgrade:
			return "Incompatible Subsonic REST protocol version. Server must upgrade.";
		case Error::Code::WrongUsernameOrPassword:
			return "Wrong username or password.";
		case Error::Code::UserNotAuthorized:
			return "User is not authorized for the given operation.";
		case Error::Code::RequestedDataNotFound:
			return "The requested data was not found.";
		default:
			return "Unknown error";
	}
}

Error::Error(Code code)
: _code {code},
_message {ErrorCodeToString(code)}
{
}

Error::Error(const std::string& message)
: _code {Code::Generic},
_message {message}
{
}

void
Response::Node::setAttribute(const std::string& key, const std::string& value)
{
	attributes[key] = value;
}

void
Response::Node::addChild(const std::string& key, Node node)
{
	children[key].emplace_back(std::move(node));
}

void
Response::Node::addArrayChild(const std::string& key, Node node)
{
	childrenArrays[key].emplace_back(std::move(node));
}


Response::Node&
Response::Node::createChild(const std::string& key)
{
	children[key].emplace_back();
	return children[key].back();
}

Response::Node&
Response::Node::createArrayChild(const std::string& key)
{
	childrenArrays[key].emplace_back();
	return childrenArrays[key].back();
}

Response
Response::createOkResponse()
{
	Response response;
	Node& responseNode {response._root.createChild("subsonic-response")};

	responseNode.setAttribute("status", "ok");
	responseNode.setAttribute("version", API_VERSION);

	return response;
}

Response
Response::createFailedResponse(const Error& error)
{
	Response response;
	Node& responseNode {response._root.createChild("subsonic-response")};

	responseNode.setAttribute("status", "failed");
	responseNode.setAttribute("version", API_VERSION);

	Node& errorNode {responseNode.createChild("error")};
	errorNode.setAttribute("code", std::to_string(static_cast<int>(error.getCode())));
	errorNode.setAttribute("message", error.getMessage());

	return response;
}

Response::Node&
Response::createNode(const std::string& key)
{
	return _root.children["subsonic-response"].front().createChild(key);
}

boost::property_tree::ptree
NodeToPropertyTree(const Response::Node& node, ResponseFormat format)
{
	boost::property_tree::ptree res;

	for (auto itChildNode : node.children)
	{
		for (const Response::Node& childNode : itChildNode.second)
			res.add_child(itChildNode.first, NodeToPropertyTree(childNode, format));
	}

	for (auto itChildArrayNode : node.childrenArrays)
	{
		const std::vector<Response::Node>& childArrayNodes {itChildArrayNode .second};

		if (format == ResponseFormat::json)
		{
			boost::property_tree::ptree array;

			for (const Response::Node& childNode : childArrayNodes )
				array.push_back(std::make_pair("", NodeToPropertyTree(childNode, format)));

			res.add_child(itChildArrayNode.first, array);
		}
		else
		{
			for (const Response::Node& childNode : childArrayNodes )
				res.add_child(itChildArrayNode.first, NodeToPropertyTree(childNode, format));
		}
	}

	for (auto itAttribute : node.attributes)
	{
		std::string key {format == ResponseFormat::xml ? "<xmlattr>." : ""};

		key += itAttribute.first;

		res.put(key, itAttribute.second);
	}

	return res;
}

void responseToStream(const Response& response, ResponseFormat format, std::ostream& os)
{
	boost::property_tree::ptree root {NodeToPropertyTree(response._root, format)};

	switch (format)
	{
		case ResponseFormat::xml:
			boost::property_tree::write_xml(os, root);
			break;
		case ResponseFormat::json:
			boost::property_tree::write_json(os, root);
			break;

	}
}

} // namespace

