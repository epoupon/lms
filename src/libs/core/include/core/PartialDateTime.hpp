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
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Wt
{
    class WDateTime;
}

namespace lms::core
{
    class PartialDateTime
    {
    public:
        constexpr PartialDateTime() = default;
        PartialDateTime(int year);
        PartialDateTime(int year, unsigned month);
        PartialDateTime(int year, unsigned month, unsigned day);
        PartialDateTime(int year, unsigned month, unsigned day, unsigned hour, unsigned min, unsigned sec);

        static PartialDateTime fromString(std::string_view str);
        static PartialDateTime fromWtDateTime(const Wt::WDateTime& dateTime);
        std::string toString() const;

        constexpr bool isValid() const { return _precision != Precision::Invalid; }

        enum class Precision : std::uint8_t
        {
            Invalid,
            Year,
            Month,
            Day,
            Hour,
            Min,
            Sec,
        };
        constexpr Precision getPrecision() const { return _precision; }

        constexpr std::optional<int> getYear() const { return (_precision >= Precision::Year ? std::make_optional(_year) : std::nullopt); }
        constexpr std::optional<int> getMonth() const { return (_precision >= Precision::Month ? std::make_optional(_month) : std::nullopt); }
        constexpr std::optional<int> getDay() const { return (_precision >= Precision::Day ? std::make_optional(_day) : std::nullopt); }
        constexpr std::optional<int> getHour() const { return (_precision >= Precision::Hour ? std::make_optional(_hour) : std::nullopt); }
        constexpr std::optional<int> getMin() const { return (_precision >= Precision::Min ? std::make_optional(_min) : std::nullopt); }
        constexpr std::optional<int> getSec() const { return (_precision >= Precision::Sec ? std::make_optional(_sec) : std::nullopt); }

        constexpr auto operator<=>(const PartialDateTime& other) const = default;

    private:
        static PartialDateTime parseDateTime(const char* format, const std::string& dateTimeStr);

        std::int16_t _year{};
        std::uint8_t _month{}; // 1 to 12
        std::uint8_t _day{};   // 1 to 31
        std::uint8_t _hour{};  // 0 to 23
        std::uint8_t _min{};   // 0 to 59
        std::uint8_t _sec{};   // 0 to 59
        Precision _precision{ Precision::Invalid };
    };
} // namespace lms::core