/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "AuthUtils.hpp"
#include "SubsonicResponse.hpp"

namespace lms::api::subsonic::utils::tests
{
    // Reference values from the Subsonic API documentation (apiKey = "sesame", salt = "c19b2d")
    TEST(AuthUtils, checkAuthToken_ValidToken)
    {
        EXPECT_TRUE(checkAuthToken("sesame", "c19b2d", "26719a1196d2a940705a59634eb18eab"));
    }

    TEST(AuthUtils, checkAuthToken_ValidToken_UppercaseHex)
    {
        EXPECT_TRUE(checkAuthToken("sesame", "c19b2d", "26719A1196D2A940705A59634EB18EAB"));
    }

    TEST(AuthUtils, checkAuthToken_WrongToken)
    {
        EXPECT_FALSE(checkAuthToken("sesame", "c19b2d", "00000000000000000000000000000000"));
    }

    TEST(AuthUtils, checkAuthToken_WrongSalt)
    {
        EXPECT_FALSE(checkAuthToken("sesame", "differentsalt", "26719a1196d2a940705a59634eb18eab"));
    }

    TEST(AuthUtils, checkAuthToken_WrongApiKey)
    {
        EXPECT_FALSE(checkAuthToken("wrongkey", "c19b2d", "26719a1196d2a940705a59634eb18eab"));
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_apiKeyOnly)
    {
        const auto request{ parseAndValidateAuthenticationRequest({ { "apiKey", { "apiKey" } } }) };
        ASSERT_TRUE(std::holds_alternative<ApiKeyAuthentication>(request));
        EXPECT_EQ(std::get<ApiKeyAuthentication>(request).apiKey, "apiKey");

        EXPECT_THROW(parseAndValidateAuthenticationRequest({}), RequiredParameterMissingError);
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_password)
    {
        const auto request{ parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "p", { "password" } } }) };
        ASSERT_TRUE(std::holds_alternative<PasswordAuthentication>(request));
        EXPECT_EQ(std::get<PasswordAuthentication>(request).user, "user");
        EXPECT_EQ(std::get<PasswordAuthentication>(request).password, "password");

        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "p", { "password" } } }), RequiredParameterMissingError); // missing u
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } } }), RequiredParameterMissingError);     // missing p
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_token)
    {
        const auto request{ parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "t", { "token" } }, { "s", { "salt" } } }) };
        ASSERT_TRUE(std::holds_alternative<TokenAuthentication>(request));
        EXPECT_EQ(std::get<TokenAuthentication>(request).user, "user");
        EXPECT_EQ(std::get<TokenAuthentication>(request).token, "token");
        EXPECT_EQ(std::get<TokenAuthentication>(request).salt, "salt");

        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "t", { "token" } }, { "s", { "salt" } } }), RequiredParameterMissingError); // missing u
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "s", { "salt" } } }), RequiredParameterMissingError);  // missing t
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "t", { "token" } } }), RequiredParameterMissingError); // missing s
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_conflicts)
    {
        // password + token
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "p", { "password" } }, { "t", { "token" } }, { "s", { "salt" } } }), MultipleConflictingAuthenticationMechanismsProvidedError);
        // apiKey + password
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "p", { "password" } }, { "apiKey", { "apiKey" } } }), MultipleConflictingAuthenticationMechanismsProvidedError);
        // apiKey + token
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } }, { "t", { "token" } }, { "s", { "salt" } }, { "apiKey", { "apiKey" } } }), MultipleConflictingAuthenticationMechanismsProvidedError);
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_usernameOnlyRequiresPassword)
    {
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "u", { "user" } } }), RequiredParameterMissingError); // missing p
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_apiKeyWithPartialPassword)
    {
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "p", { "password" } }, { "apiKey", { "apiKey" } } }), RequiredParameterMissingError); // missing u
    }

    TEST(AuthUtils, parseAndValidateAuthenticationRequest_apiKeyWithPartialToken)
    {
        EXPECT_THROW(parseAndValidateAuthenticationRequest({ { "s", { "salt" } }, { "apiKey", { "apiKey" } } }), RequiredParameterMissingError); // missing u
    }
} // namespace lms::api::subsonic::utils::tests
