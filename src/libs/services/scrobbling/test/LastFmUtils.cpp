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

#include <gtest/gtest.h>

#include "lastfm/Utils.hpp"

namespace lms::scrobbling::lastFm::utils::tests
{
    TEST(LastFmUtils, computeApiSig_empty_params)
    {
        // empty params: payload = secret only
        EXPECT_EQ(computeApiSig({}, "mysecret"), "06c219e5bc8378f3a8a3f83b4b7e4649");
    }

    TEST(LastFmUtils, computeApiSig_basic)
    {
        const std::map<std::string, std::string> params{ { "api_key", "testkey" } };
        // payload = "api_keytestkeymysecret"
        EXPECT_EQ(computeApiSig(params, "mysecret"), "20e219ce486aa95fc1e3a72c09f1587c");
    }

    TEST(LastFmUtils, computeApiSig_format_excluded)
    {
        const std::map<std::string, std::string> params{
            { "api_key", "testkey" },
            { "format", "json" },
        };
        // "format" is excluded, same result as without it
        EXPECT_EQ(computeApiSig(params, "mysecret"), "20e219ce486aa95fc1e3a72c09f1587c");
    }

    TEST(LastFmUtils, computeApiSig_callback_excluded)
    {
        const std::map<std::string, std::string> params{
            { "api_key", "testkey" },
            { "callback", "fn" },
            { "format", "json" },
        };
        // both "callback" and "format" excluded, same result
        EXPECT_EQ(computeApiSig(params, "mysecret"), "20e219ce486aa95fc1e3a72c09f1587c");
    }

    TEST(LastFmUtils, computeApiSig_params_sorted_by_key)
    {
        const std::map<std::string, std::string> params{
            { "api_key", "testkey" },
            { "method", "track.love" },
            { "track", "song title" },
        };
        // payload = "api_keytestkey" + "methodtrack.love" + "tracksong title" + "secret"
        EXPECT_EQ(computeApiSig(params, "secret"), "6c6f4099790ab9e72d59fdd0632a8a81");
    }

    TEST(LastFmUtils, buildFormBody_empty)
    {
        EXPECT_EQ(buildFormBody({}), "");
    }

    TEST(LastFmUtils, buildFormBody_single_param)
    {
        const std::map<std::string, std::string> params{ { "key", "value" } };
        EXPECT_EQ(buildFormBody(params), "key=value");
    }

    TEST(LastFmUtils, buildFormBody_multiple_params_sorted)
    {
        const std::map<std::string, std::string> params{
            { "a", "1" },
            { "b", "2" },
        };
        EXPECT_EQ(buildFormBody(params), "a=1&b=2");
    }

    TEST(LastFmUtils, buildFormBody_encodes_space)
    {
        const std::map<std::string, std::string> params{ { "track", "hello world" } };
        EXPECT_EQ(buildFormBody(params), "track=hello%20world");
    }

    TEST(LastFmUtils, buildFormBody_encodes_special_chars)
    {
        const std::map<std::string, std::string> params{ { "q", "foo=bar&baz" } };
        EXPECT_EQ(buildFormBody(params), "q=foo%3Dbar%26baz");
    }

    TEST(LastFmUtils, parseAuthToken_valid)
    {
        EXPECT_EQ(parseAuthToken(R"({"token":"abc123"})"), "abc123");
    }

    TEST(LastFmUtils, parseAuthToken_empty_body)
    {
        EXPECT_EQ(parseAuthToken(""), "");
    }

    TEST(LastFmUtils, parseAuthToken_invalid_json)
    {
        EXPECT_EQ(parseAuthToken("not json"), "");
    }

    TEST(LastFmUtils, parseAuthToken_missing_key)
    {
        EXPECT_EQ(parseAuthToken(R"({"other":"value"})"), "");
    }

    TEST(LastFmUtils, parseSessionKey_valid)
    {
        EXPECT_EQ(parseSessionKey(R"({"session":{"key":"xyz789","name":"user"}})"), "xyz789");
    }

    TEST(LastFmUtils, parseSessionKey_empty_body)
    {
        EXPECT_EQ(parseSessionKey(""), "");
    }

    TEST(LastFmUtils, parseSessionKey_invalid_json)
    {
        EXPECT_EQ(parseSessionKey("not json"), "");
    }

    TEST(LastFmUtils, parseSessionKey_missing_session)
    {
        EXPECT_EQ(parseSessionKey(R"({"other":"value"})"), "");
    }

    TEST(LastFmUtils, parseSessionKey_missing_key_in_session)
    {
        EXPECT_EQ(parseSessionKey(R"({"session":{"name":"user"}})"), "");
    }
} // namespace lms::scrobbling::lastFm::utils::tests
