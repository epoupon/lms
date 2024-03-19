/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <thread>
#include <benchmark/benchmark.h>

#include "SubsonicResponse.hpp"

namespace lms::api::subsonic::benchs
{
    namespace
    {
        Response generateFakeResponse()
        {
            Response response{ Response::createOkResponse(defaultServerProtocolVersion) };

            Response::Node& node{ response.createNode("MyNode") };
            node.setAttribute("Attr1", "value1");
            node.setAttribute("Attr2", "value2");

            for (std::size_t i{}; i < 100; ++i)
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
    }

    static void BM_SubsonicResponse_generate(benchmark::State& state)
    {
        for (auto _ : state)
        {
            Response response{ generateFakeResponse() };
            benchmark::DoNotOptimize(response);
            TLSMonotonicMemoryResource::getInstance().reset();
        }
    }

    template <ResponseFormat responseFormat>
    static void BM_SubsonicResponse_serialize(benchmark::State& state)
    {
        const Response response{ generateFakeResponse() };

        for (auto _ : state)
        {
            std::ostringstream oss; // TODO find something more optimized
            response.write(oss, responseFormat);
        }
    }

    BENCHMARK(BM_SubsonicResponse_generate)->Threads(1)->Threads(std::thread::hardware_concurrency());
    BENCHMARK(BM_SubsonicResponse_serialize<ResponseFormat::json>);
    BENCHMARK(BM_SubsonicResponse_serialize<ResponseFormat::xml>);
}

BENCHMARK_MAIN();