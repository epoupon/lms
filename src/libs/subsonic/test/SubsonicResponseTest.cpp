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

#include <sstream>

#include <gtest/gtest.h>

#include "ProtocolVersion.hpp"
#include "SubsonicResponse.hpp"

namespace lms::api::subsonic::tests
{
    namespace
    {
        Response generateFakeResponse()
        {
            Response response{ Response::createOkResponse(defaultServerProtocolVersion) };

            Response::Node& node{ response.createNode("MyNode") };
            node.setAttribute("Attr1", "value1");
            node.setAttribute("Attr2", "value2");
            node.setAttribute("attr3", "<value3=\"foo\">");
            node.setAttribute("attr4", true);
            node.setAttribute("attr5", false);
            node.setAttribute("attr6", 3.14159265359);
            node.setAttribute("attr7", 333666);

            for (std::size_t i{}; i < 2; ++i)
            {
                Response::Node& childNode{ node.createArrayChild("MyArrayChild") };
                childNode.setAttribute("Attr42", i);

                node.addArrayValue("MyArray1", "value1");
                node.addArrayValue("MyArray1", "value2");
                for (std::size_t j{}; j < i; ++j)
                    node.addArrayValue("MyArray2", j);
            }

            return response;
        }
    } // namespace

    TEST(SubsonicResponse, emptyJson)
    {
        Response response{ Response::createOkResponse(ProtocolVersion{ 1, 16, 0 }) };

        std::ostringstream oss;
        response.write(oss, ResponseFormat::json);

        EXPECT_EQ(oss.str(), R"({"subsonic-response":{"openSubsonic":true,"serverVersion":"v3.72.0","status":"ok","type":"lms","version":"1.16.0"}})");
    }

    TEST(SubsonicResponse, json)
    {
        Response response{ generateFakeResponse() };

        std::ostringstream oss;
        response.write(oss, ResponseFormat::json);

        EXPECT_EQ(oss.str(), R"({"subsonic-response":{"openSubsonic":true,"serverVersion":"v3.72.0","status":"ok","type":"lms","version":"1.16.0","MyNode":{"Attr1":"value1","Attr2":"value2","attr3":"<value3=\"foo\">","attr4":true,"attr5":false,"attr6":3.14159,"attr7":333666,"MyArrayChild":[{"Attr42":0},{"Attr42":1}],"MyArray1":["value1","value2","value1","value2"],"MyArray2":[0]}}})");
    }

    TEST(SubsonicResponse, emptyXml)
    {
        Response response{ Response::createOkResponse(ProtocolVersion{ 1, 16, 0 }) };

        std::ostringstream oss;
        response.write(oss, ResponseFormat::xml);

        EXPECT_EQ(oss.str(), R"(<?xml version="1.0" encoding="utf-8"?>
<subsonic-response openSubsonic="true" serverVersion="v3.72.0" status="ok" type="lms" version="1.16.0" xmlns="http://subsonic.org/restapi"/>)");
    }

    TEST(SubsonicResponse, xml)
    {
        Response response{ generateFakeResponse() };

        std::ostringstream oss;
        response.write(oss, ResponseFormat::xml);

        EXPECT_EQ(oss.str(), R"(<?xml version="1.0" encoding="utf-8"?>
<subsonic-response openSubsonic="true" serverVersion="v3.72.0" status="ok" type="lms" version="1.16.0" xmlns="http://subsonic.org/restapi"><MyNode Attr1="value1" Attr2="value2" attr3="&lt;value3=&quot;foo&quot;&gt;" attr4="true" attr5="false" attr6="3.14159" attr7="333666"><MyArrayChild Attr42="0"/><MyArrayChild Attr42="1"/><MyArray1>value1</MyArray1><MyArray1>value2</MyArray1><MyArray1>value1</MyArray1><MyArray1>value2</MyArray1><MyArray2>0</MyArray2></MyNode></subsonic-response>)");
    }

} // namespace lms::api::subsonic::tests

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}