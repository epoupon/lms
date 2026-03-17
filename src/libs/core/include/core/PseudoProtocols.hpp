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

#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <variant>
#include <vector>

namespace lms::core
{
    class IPseudoProtocol
    {
    public:
        IPseudoProtocol(const std::filesystem::path& root)
            : _root{ root }
        {
            protocols.push_back(this);
        }

        virtual ~IPseudoProtocol() = default;

        [[nodiscard]]
        static std::vector<IPseudoProtocol*> getAll()
        { return protocols; }

        [[nodiscard]]
        const std::filesystem::path& getRoot() const
        { return _root; }

        [[nodiscard]]
        bool matches(const std::filesystem::path& path) const
        { return std::string_view{path.c_str()}.starts_with(_root.c_str()); }

        [[nodiscard]]
        static bool matchAny(const std::filesystem::path& path)
        {
            return std::ranges::any_of(
                protocols,
                [&path] (auto* p) { return p->matches(path); }
            );
        }

    private:
        std::filesystem::path _root;
        inline static std::vector<IPseudoProtocol*> protocols;
    };

    inline const struct TrackOn : public IPseudoProtocol
    {
        struct DecipheredURI
        {
            std::filesystem::path     path;
            std::chrono::milliseconds start;
            std::chrono::milliseconds duration;
        };

        TrackOn() : IPseudoProtocol{ "track_on:/" } {}

        [[nodiscard]] std::variant<DecipheredURI, std::string>
        parseUri(const std::filesystem::path& path) const
        {
            if (!matches(path)) {
                return std::string{ "Path does not match the protocol" };
            }

            std::string_view p{ path.c_str() };
            p = p.substr(std::string_view{ getRoot().c_str() }.size());

            const auto h_pos = p.rfind('#');
            if (h_pos == std::string_view::npos) {
                return std::string{ "Path does not contain '#'" };
            }

            const auto data  = p.substr(h_pos + 1);
            const auto d_pos = data.find('-');
            if (d_pos == std::string_view::npos) {
                return std::string{ "Data is not a dash-separated pair of integers" };
            }

            const std::string start   { data.substr(0, d_pos)  };
            const std::string duration{ data.substr(d_pos + 1) };
            static constexpr auto* digits = "0123456789";
            if (
                start.find_first_not_of(digits) != std::string::npos
                || duration.find_first_not_of(digits) != std::string::npos
            ) {
                return std::string{ "Data is not a dash-separated pair of integers" };
            }

            return DecipheredURI{
                .path     = std::filesystem::path{"/"} / p.substr(0, h_pos),
                .start    = std::chrono::milliseconds{ std::stoll(start) },
                .duration = std::chrono::milliseconds{ std::stoll(duration) },
            };
        }

        [[nodiscard]]
        std::filesystem::path encode(const DecipheredURI& dURI) const
        {
            return getRoot() / (
                dURI.path.relative_path().string()
                + "#"
                + std::to_string(dURI.start.count())
                + "-"
                + std::to_string(dURI.duration.count())
            );
        }

    } track_on {};

    [[nodiscard]] inline bool protocolPath(const std::filesystem::path& path)
    { return IPseudoProtocol::matchAny(path); }
} // namespace lms::core
