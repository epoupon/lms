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

#include "SubsonicResourceConfig.hpp"

#include "core/IConfig.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        std::unordered_map<std::string, ProtocolVersion> readConfigProtocolVersions(core::IConfig& config)
        {
            std::unordered_map<std::string, ProtocolVersion> res;

            config.visitStrings("api-subsonic-old-server-protocol-clients",
                                [&](std::string_view client) {
                                    res.emplace(std::string{ client }, ProtocolVersion{ .major = 1, .minor = 12, .patch = 0 });
                                },
                                { "DSub" });

            return res;
        }

        std::unordered_set<std::string> readOpenSubsonicDisabledClients(core::IConfig& config)
        {
            std::unordered_set<std::string> res;

            config.visitStrings("api-open-subsonic-disabled-clients",
                                [&](std::string_view client) {
                                    res.emplace(std::string{ client });
                                },
                                { "DSub" });

            return res;
        }
    } // namespace

    SubsonicResourceConfig readSubsonicResourceConfig(core::IConfig& config)
    {
        return SubsonicResourceConfig{
            .serverProtocolVersionsByClient = readConfigProtocolVersions(config),
            .openSubsonicDisabledClients = readOpenSubsonicDisabledClients(config),
            .supportUserPasswordAuthentication = config.getBool("api-subsonic-support-user-password-auth", true)
        };
    }
} // namespace lms::api::subsonic