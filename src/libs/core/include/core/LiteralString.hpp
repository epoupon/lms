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

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

namespace lms::core
{
    class LiteralString
    {
    public:
        constexpr LiteralString() noexcept = default;
        template<std::size_t N>
        constexpr LiteralString(const char(&str)[N]) noexcept : _str{ str, N - 1 } { static_assert(N > 0); }

        constexpr bool empty() const noexcept { return _str.empty(); }
        constexpr const char* c_str() const noexcept { return _str.data(); }
        constexpr std::size_t length() const noexcept { return _str.length(); }
        constexpr std::string_view str() const noexcept { return _str; }
        constexpr auto operator<=>(const LiteralString& other) const = default;

    private:
        std::string_view _str;
    };

    inline std::ostream& operator<<(std::ostream& os, const LiteralString& str)
    {
        os << str.str();
        return os;
    }
}

namespace std
{
    template<>
    struct hash<lms::core::LiteralString>
    {
        size_t operator()(const lms::core::LiteralString& str) const
        {
            return hash<std::string_view>{}(str.str());
        }
    };
}

namespace lms::core
{
    struct LiteralStringHash
    {
        using hash_type = std::hash<std::string_view>;
        using is_transparent = void;

        [[nodiscard]] size_t operator()(const LiteralString& str) const {
            return hash_type{}(str.str());
        }

        [[nodiscard]] size_t operator()(std::string_view str) const {
            return hash_type{}(str);
        }

        [[nodiscard]] size_t operator()(const std::string& str) const {
            return hash_type{}(str);
        }
    };

    struct LiteralStringEqual
    {
        using is_transparent = void;

        [[nodiscard]] bool operator()(const LiteralString& lhs, const LiteralString& rhs) const {
            return lhs == rhs;
        }

        [[nodiscard]] bool operator()(const LiteralString& lhs, const std::string& rhs) const {
            return lhs.str() == rhs;
        }

        [[nodiscard]] bool operator()(const LiteralString& lhs, std::string_view rhs) const {
            return lhs.str() == rhs;
        }

        [[nodiscard]] bool operator()(const std::string_view& lhs, LiteralString rhs) const {
            return lhs == rhs.str();
        }

        [[nodiscard]] bool operator()(const std::string& lhs, const LiteralString& rhs) const {
            return lhs == rhs.str();
        }
    };
}