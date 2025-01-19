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
#include "core/PartialDateTime.hpp"

#include <iomanip>
#include <limits>
#include <sstream>

namespace lms::core
{
    PartialDateTime::PartialDateTime(int year)
        : _year{ static_cast<std::int16_t>(year) }
        , _precision{ Precision::Year }
    {
    }

    PartialDateTime::PartialDateTime(int year, unsigned month)
        : _year{ static_cast<std::int16_t>(year) }
        , _month{ static_cast<std::uint8_t>(month) }
        , _precision{ Precision::Month }
    {
    }

    PartialDateTime::PartialDateTime(int year, unsigned month, unsigned day)
        : _year{ static_cast<std::int16_t>(year) }
        , _month{ static_cast<std::uint8_t>(month) }
        , _day{ static_cast<std::uint8_t>(day) }
        , _precision{ Precision::Day }
    {
    }

    PartialDateTime::PartialDateTime(int year, unsigned month, unsigned day, unsigned hour, unsigned min, unsigned sec)
        : _year{ static_cast<std::int16_t>(year) }
        , _month{ static_cast<std::uint8_t>(month) }
        , _day{ static_cast<std::uint8_t>(day) }
        , _hour{ static_cast<std::uint8_t>(hour) }
        , _min{ static_cast<std::uint8_t>(min) }
        , _sec{ static_cast<std::uint8_t>(sec) }
        , _precision{ Precision::Sec }
    {
    }

    PartialDateTime PartialDateTime::fromString(std::string_view str)
    {
        PartialDateTime res;

        const std::string dateTimeStr{ str };

        static constexpr const char* formats[]{
            "%Y-%m-%dT%H:%M:%S",
            "%Y-%m-%d %H:%M:%S",
            "%Y/%m/%d %H:%M:%S",
        };

        for (const char* format : formats)
        {
            PartialDateTime candidate;

            std::tm tm{};
            tm.tm_year = std::numeric_limits<decltype(tm.tm_year)>::min();
            tm.tm_mon = std::numeric_limits<decltype(tm.tm_mon)>::min();
            tm.tm_mday = std::numeric_limits<decltype(tm.tm_mday)>::min();
            tm.tm_hour = std::numeric_limits<decltype(tm.tm_hour)>::min();
            tm.tm_min = std::numeric_limits<decltype(tm.tm_min)>::min();
            tm.tm_sec = std::numeric_limits<decltype(tm.tm_sec)>::min();

            std::istringstream ss{ dateTimeStr };
            ss >> std::get_time(&tm, format);
            if (ss.fail())
                continue;

            if (tm.tm_sec != std::numeric_limits<decltype(tm.tm_sec)>::min())
            {
                candidate._sec = tm.tm_sec;
                candidate._precision = Precision::Sec;
            }
            if (tm.tm_min != std::numeric_limits<decltype(tm.tm_min)>::min())
            {
                candidate._min = tm.tm_min;
                if (candidate._precision == Precision::Invalid)
                    candidate._precision = Precision::Min;
            }
            if (tm.tm_hour != std::numeric_limits<decltype(tm.tm_hour)>::min())
            {
                candidate._hour = tm.tm_hour;
                if (candidate._precision == Precision::Invalid)
                    candidate._precision = Precision::Hour;
            }
            if (tm.tm_mday != std::numeric_limits<decltype(tm.tm_mday)>::min())
            {
                candidate._day = tm.tm_mday;
                if (candidate._precision == Precision::Invalid)
                    candidate._precision = Precision::Day;
            }
            if (tm.tm_mon != std::numeric_limits<decltype(tm.tm_mon)>::min())
            {
                candidate._month = tm.tm_mon + 1; // tm.tm_mon is [0, 11]
                if (candidate._precision == Precision::Invalid)
                    candidate._precision = Precision::Month;
            }
            if (tm.tm_year != std::numeric_limits<decltype(tm.tm_year)>::min())
            {
                candidate._year = tm.tm_year + 1900; // tm.tm_year is years since 1900
                if (candidate._precision == Precision::Invalid)
                    candidate._precision = Precision::Year;
            }

            if (candidate > res)
                res = candidate;

            if (res._precision == Precision::Sec)
                break;
        }

        return res;
    }

    std::string PartialDateTime::toISO8601String() const
    {
        if (_precision == Precision::Invalid)
            return "";

        std::ostringstream ss;

        ss << std::setfill('0') << std::setw(4) << _year;
        if (_precision >= Precision::Month)
            ss << "-" << std::setw(2) << static_cast<int>(_month);
        if (_precision >= Precision::Day)
            ss << "-" << std::setw(2) << static_cast<int>(_day);
        if (_precision >= Precision::Hour)
            ss << 'T' << std::setw(2) << static_cast<int>(_hour);
        if (_precision >= Precision::Min)
            ss << ':' << std::setw(2) << static_cast<int>(_min);
        if (_precision >= Precision::Sec)
            ss << ':' << std::setw(2) << static_cast<int>(_sec);

        return ss.str();
    }
} // namespace lms::core
