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
#include <cmath>
#include <climits>
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

    void Response::Node::setAttribute(Key key, std::string_view value)
    {
        _attributes[key] = std::string{ value };
    }

    void Response::Node::addChild(Key key, Node&& node)
    {
        assert(!_value);
        assert(_children.find(key) == std::cend(_children));
        _children[key] = std::move(node);
    }

    void Response::Node::createEmptyArrayChild(Key key)
    {
        assert(!_value);
        assert(_children.find(key) == std::cend(_children));
        _childrenArrays.emplace(key, std::vector<Node>{});
    }

    void Response::Node::addArrayChild(Key key, Node&& node)
    {
        assert(!_value);
        assert(_children.find(key) == std::cend(_children));
        _childrenArrays[key].emplace_back(std::move(node));
    }

    void Response::Node::createEmptyArrayValue(Key key)
    {
        assert(!_value);
        assert(_children.find(key) == std::cend(_children));
        _childrenValues.emplace(key, ValuesType{});
    }

    void Response::Node::addArrayValue(Key key, std::string_view value)
    {
        assert(!_value);
        assert(_children.find(key) == std::cend(_children));
        auto& values{ _childrenValues[key] };
        values.push_back(std::string{ value });
        assert(std::all_of(std::cbegin(values) + 1, std::cend(values), [&](const ValueType& value) {return value.index() == values.front().index();}));
    }

    void Response::Node::addArrayValue(Key key, long long value)
    {
        assert(!_value);
        auto& values{ _childrenValues[key] };
        values.push_back(value);
        assert(std::all_of(std::cbegin(values) + 1, std::cend(values), [&](const ValueType& value) {return value.index() == values.front().index();}));
    }

    Response::Node& Response::Node::createChild(Key key)
    {
        assert(!_value);
        return _children[key];
    }

    Response::Node& Response::Node::createArrayChild(Key key)
    {
        assert(!_value);
        assert(_children.find(key) == std::cend(_children));
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

    void Response::addNode(Node::Key key, Node&& node)
    {
        return _root._children["subsonic-response"].addChild(key, std::move(node));
    }

    Response::Node& Response::createNode(Node::Key key)
    {
        return _root._children["subsonic-response"].createChild(key);
    }

    Response::Node& Response::createArrayNode(Node::Key key)
    {
        return _root._children["subsonic-response"].createArrayChild(key);
    }

    void Response::write(std::ostream& os, ResponseFormat format) const
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

    void Response::writeXML(std::ostream& os) const
    {
        std::function<boost::property_tree::ptree(const Node&)> nodeToPropertyTree = [&](const Node& node)
            {
                boost::property_tree::ptree res;

                for (const auto& [key, value] : node._attributes)
                {
                    if (std::holds_alternative<std::string>(value))
                        res.put("<xmlattr>." + std::string{ key.get() }, std::get<std::string>(value));
                    else if (std::holds_alternative<bool>(value))
                        res.put("<xmlattr>." + std::string{ key.get() }, std::get<bool>(value));
                    else if (std::holds_alternative<float>(value))
                        res.put("<xmlattr>." + std::string{ key.get() }, std::get<float>(value));
                    else if (std::holds_alternative<long long>(value))
                        res.put("<xmlattr>." + std::string{ key.get() }, std::get<long long>(value));
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
                    for (const auto& [key, childNode] : node._children)
                    {
                        res.add_child(std::string{ key.get() }, nodeToPropertyTree(childNode));
                    }

                    for (const auto& [key, childArrayNodes] : node._childrenArrays)
                    {
                        for (const Node& childNode : childArrayNodes)
                            res.add_child(std::string{ key.get() }, nodeToPropertyTree(childNode));
                    }

                    for (const auto& [key, childArrayValues] : node._childrenValues)
                    {
                        for (const Response::Node::ValueType& value : childArrayValues)
                            res.add_child(std::string{ key.get() }, valueToPropertyTree(value));
                    }
                }

                return res;
            };

        boost::property_tree::ptree root{ nodeToPropertyTree(_root) };
        boost::property_tree::write_xml(os, root);
    }

    void Response::JsonSerializer::serializeNode(std::ostream& os, const Response::Node& node)
    {
        os << '{';

        bool first{ true };

        for (const auto& [key, value] : node._attributes)
        {
            if (!first)
                os << ',';

            serializeEscapedString(os, key.get());
            os << ':';
            serializeValue(os, value);

            first = false;
        }

        if (node._value)
        {
            if (!first)
                os << ',';

            os << "\"value\":";
            serializeValue(os, *node._value);

            first = false;
        }
        else
        {
            for (const auto& [key, childNode] : node._children)
            {
                if (!first)
                    os << ',';

                serializeEscapedString(os, key.get());
                os << ':';
                serializeNode(os, childNode);

                first = false;
            }

            for (const auto& [key, childArrayNodes] : node._childrenArrays)
            {
                if (!first)
                    os << ',';

                serializeEscapedString(os, key.get());
                os << ":[";

                bool firstChild{ true };
                for (const Response::Node& childNode : childArrayNodes)
                {
                    if (!firstChild)
                        os << ",";

                    serializeNode(os, childNode);
                    firstChild = false;
                }
                os << ']';

                first = false;
            }

            for (const auto& [key, childValues] : node._childrenValues)
            {
                if (!first)
                    os << ',';

                serializeEscapedString(os, key.get());
                os << ":[";

                bool firstChild{ true };
                for (const Node::ValueType& childValue : childValues)
                {
                    if (!firstChild)
                        os << ",";

                    serializeValue(os, childValue);

                    firstChild = false;
                }
                os << ']';

                first = false;
            }
        }

        os << '}';
    }

    void Response::JsonSerializer::serializeValue(std::ostream& os, const Node::ValueType& value)
    {
        if (std::holds_alternative<std::string>(value))
        {
            serializeEscapedString(os, std::get<std::string>(value));
        }
        else if (std::holds_alternative<bool>(value))
        {
            os << (std::get<bool>(value) ? "true" : "false");
        }
        else if (std::holds_alternative<float>(value))
        {
            const float d{ std::get<float>(value) };
            if (std::isnan(d) || std::fabs(d) == std::numeric_limits<float>::infinity())
                os << "null";
            else
                os << d;
        }
        else if (std::holds_alternative<long long>(value))
        {
            os << std::get<long long>(value);
        }
        else
        {
            assert(false);
        }
    }

    void Response::JsonSerializer::serializeEscapedString(std::ostream& os, std::string_view str)
    {
        os << '\"';
        StringUtils::writeJsonEscapedString(os, str);
        os << '\"';
    }

    void Response::writeJSON(std::ostream& os) const
    {
        JsonSerializer serializer;
        serializer.serializeNode(os, _root);
    }

} // namespace
