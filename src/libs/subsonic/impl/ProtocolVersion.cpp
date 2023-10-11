/*
 * copyright (c) 2021 emeric poupon
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

#include "ProtocolVersion.hpp"

namespace StringUtils
{
    template<>
    std::optional<API::Subsonic::ProtocolVersion> readAs(std::string_view str)
    {
        // Expects "X.Y.Z"
        const auto numbers{ StringUtils::splitString(str, ".") };
        if (numbers.size() < 2 || numbers.size() > 3)
            return std::nullopt;

        API::Subsonic::ProtocolVersion version;

        auto number{ StringUtils::readAs<unsigned>(numbers[0]) };
        if (!number)
            return std::nullopt;
        version.major = *number;

        number = { StringUtils::readAs<unsigned>(numbers[1]) };
        if (!number)
            return std::nullopt;
        version.minor = *number;

        if (numbers.size() == 3)
        {
            number = { StringUtils::readAs<unsigned>(numbers[2]) };
            if (!number)
                return std::nullopt;
            version.patch = *number;
        }

        return version;
    }
}

