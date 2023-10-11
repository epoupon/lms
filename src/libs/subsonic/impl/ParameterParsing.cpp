/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ParameterParsing.hpp"

namespace API::Subsonic
{
    bool hasParameter(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
    {
        return parameterMap.find(param) != std::cend(parameterMap);
    }

    std::string decodePasswordIfNeeded(const std::string& password)
    {
        if (password.find("enc:") == 0)
        {
            auto decodedPassword{ StringUtils::stringFromHex(password.substr(4)) };
            if (!decodedPassword)
                return password; // fallback on plain password

            return *decodedPassword;
        }

        return password;
    }
}