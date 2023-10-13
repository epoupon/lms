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

#include <cassert>
#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Serializer.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "utils/Exception.hpp"
#include "utils/String.hpp"
#include "ProtocolVersion.hpp"

namespace API::Subsonic
{
    std::string_view ResponseFormatToMimeType(ResponseFormat format)
    {
        switch (format)
        {
        case ResponseFormat::xml: return "text/xml";
        case ResponseFormat::json: return "application/json";
        }

        return "";
    }

    void Response::Node::setValue(std::string_view value)
    {
        assert(_children.empty() && _childrenArrays.empty() && _childrenValues.empty());
        _value = std::string{ value };
    }

    void Response::Node::setValue(long long value)
    {
        assert(_children.empty() && _childrenArrays.empty() && _childrenValues.empty());
        _value = value;
    }

    void Response::Node::setAttribute(std::string_view key, std::string_view value)
    {
        _attributes[std::string{ key }] = std::string{ value };
    }

    void Response::Node::addChild(const std::string& key, Node node)
    {
        assert(!_value);
        _children[key].emplace_back(std::move(node));
    }

    void Response::Node::createEmptyArrayChild(std::string_view key)
    {
        assert(!_value);
        _childrenArrays.emplace(key, std::vector<Node>{});
    }

    void Response::Node::addArrayChild(std::string_view key, Node node)
    {
        assert(!_value);
        _childrenArrays[std::string{ key }].emplace_back(std::move(node));
    }

    void Response::Node::createEmptyArrayValue(std::string_view key)
    {
        assert(!_value);
        _childrenValues.emplace(key, ValuesType{});
    }

    void Response::Node::addArrayValue(std::string_view key, std::string_view value)
    {
        assert(!_value);
        auto& values{ _childrenValues[std::string{ key }] };
        values.push_back(std::string{ value });
        assert(std::all_of(std::cbegin(values) + 1, std::cend(values), [&](const ValueType& value) {return value.index() == values.front().index();}));
    }

    void Response::Node::addArrayValue(std::string_view key, long long value)
    {
        assert(!_value);
        auto& values{ _childrenValues[std::string{ key }] };
        values.push_back(value);
        assert(std::all_of(std::cbegin(values) + 1, std::cend(values), [&](const ValueType& value) {return value.index() == values.front().index();}));
    }

    Response::Node& Response::Node::createChild(const std::string& key)
    {
        _children[key].emplace_back();
        return _children[key].back();
    }

    Response::Node& Response::Node::createArrayChild(const std::string& key)
    {
        _childrenArrays[key].emplace_back();
        return _childrenArrays[key].back();
    }

    void Response::Node::setVersionAttribute(ProtocolVersion protocolVersion)
    {
        setAttribute("version", std::to_string(protocolVersion.major) + "." + std::to_string(protocolVersion.minor) + "." + std::to_string(protocolVersion.patch));
    }

    Response Response::createOkResponse(ProtocolVersion protocolVersion)
    {
        return createResponseCommon(protocolVersion);
    }

    Response Response::createFailedResponse(ProtocolVersion protocolVersion, const Error& error)
    {
        return createResponseCommon(protocolVersion, &error);
    }

    Response Response::createResponseCommon(ProtocolVersion protocolVersion, const Error* error)
    {
        Response response;
        Node& responseNode{ response._root.createChild("subsonic-response") };

        responseNode.setAttribute("status", error ? "failed" : "ok");
        responseNode.setVersionAttribute(protocolVersion);

        if (error)
        {
            Node& errorNode{ responseNode.createChild("error") };
            errorNode.setAttribute("code", static_cast<int>(error->getCode()));
            errorNode.setAttribute("message", error->getMessage());
        }

        // OpenSubsonic mandatory fields
        responseNode.setAttribute("type", "lms");
        responseNode.setAttribute("serverVersion", serverVersion);
        responseNode.setAttribute("openSubsonic", true);

        return response;
    }

    void Response::addNode(const std::string& key, Node node)
    {
        return _root._children["subsonic-response"].front().addChild(key, std::move(node));
    }

    Response::Node& Response::createNode(const std::string& key)
    {
        return _root._children["subsonic-response"].front().createChild(key);
    }

    Response::Node& Response::createArrayNode(const std::string& key)
    {
        return _root._children["subsonic-response"].front().createArrayChild(key);
    }

    void Response::write(std::ostream& os, ResponseFormat format)
    {
        switch (format)
        {
        case ResponseFormat::xml:
            writeXML(os);
            break;
        case ResponseFormat::json:
            writeJSON(os);
            break;
        }
    }

    void Response::writeXML(std::ostream& os)
    {
        std::function<boost::property_tree::ptree(const Node&)> nodeToPropertyTree = [&](const Node& node)
            {
                boost::property_tree::ptree res;

                for (auto itAttribute : node._attributes)
                {
                    if (std::holds_alternative<std::string>(itAttribute.second))
                        res.put("<xmlattr>." + itAttribute.first, std::get<std::string>(itAttribute.second));
                    else if (std::holds_alternative<bool>(itAttribute.second))
                        res.put("<xmlattr>." + itAttribute.first, std::get<bool>(itAttribute.second));
                    else if (std::holds_alternative<float>(itAttribute.second))
                        res.put("<xmlattr>." + itAttribute.first, std::get<float>(itAttribute.second));
                    else if (std::holds_alternative<long long>(itAttribute.second))
                        res.put("<xmlattr>." + itAttribute.first, std::get<long long>(itAttribute.second));
                }

                auto valueToPropertyTree = [](const Node::ValueType& value)
                    {
                        boost::property_tree::ptree res;
                        std::visit([&](const auto& rawValue)
                            {
                                res.put_value(rawValue);
                            }, value);

                        return res;
                    };

                if (node._value)
                {
                    res = valueToPropertyTree(*node._value);
                }
                else
                {
                    for (const auto& [key, childNodes] : node._children)
                    {
                        for (const Node& childNode : childNodes)
                            res.add_child(key, nodeToPropertyTree(childNode));
                    }

                    for (const auto& [key, childArrayNodes] : node._childrenArrays)
                    {
                        for (const Node& childNode : childArrayNodes)
                            res.add_child(key, nodeToPropertyTree(childNode));
                    }

                    for (const auto& [key, childArrayValues] : node._childrenValues)
                    {
                        for (const Response::Node::ValueType& value : childArrayValues)
                            res.add_child(key, valueToPropertyTree(value));
                    }
                }

                return res;
            };

        boost::property_tree::ptree root{ nodeToPropertyTree(_root) };
        boost::property_tree::write_xml(os, root);
    }

    void Response::writeJSON(std::ostream& os)
    {
        namespace Json = Wt::Json;

        std::function<Json::Object(const Response::Node&)> nodeToJsonObject = [&](const Response::Node& node)
            {
                Json::Object res;

                auto valueToJsonValue{ [](const Node::ValueType& value) -> Json::Value
                {
                    Json::Value res;
                    std::visit([&](const auto& rawValue)
                    {
                        res = Json::Value{ rawValue };
                    }, value);
                    return res;
                } };

                for (auto itAttribute : node._attributes)
                    res[itAttribute.first] = valueToJsonValue(itAttribute.second);

                if (node._value)
                {
                    res["value"] = valueToJsonValue(*node._value);
                }
                else
                {
                    for (const auto& [key, childNodes] : node._children)
                    {
                        for (const Response::Node& childNode : childNodes)
                            res[key] = nodeToJsonObject(childNode);
                    }

                    for (const auto& [key, childArrayNodes] : node._childrenArrays)
                    {
                        Json::Array array;
                        for (const Response::Node& childNode : childArrayNodes)
                            array.emplace_back(nodeToJsonObject(childNode));

                        res[key] = std::move(array);
                    }

                    for (const auto& [key, childValues] : node._childrenValues)
                    {
                        Json::Array array;
                        for (const Node::ValueType& childValue : childValues)
                            array.emplace_back(valueToJsonValue(childValue));

                        res[key] = std::move(array);
                    }
                }

                return res;
            };

        Json::Object root{ nodeToJsonObject(_root) };
        os << Json::serialize(root);
    }

} // namespace
