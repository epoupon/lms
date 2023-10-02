/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <Wt/Http/Request.h>

#include <optional>
#include <vector>
#include <string>

#include "services/database/Types.hpp"
#include "utils/String.hpp"
#include "SubsonicResponse.hpp"

namespace API::Subsonic
{

    template<typename T>
    std::vector<T> getMultiParametersAs(const Wt::Http::ParameterMap& parameterMap, const std::string& paramName)
    {
        std::vector<T> res;

        auto it = parameterMap.find(paramName);
        if (it == parameterMap.end())
            return res;

        for (const std::string& param : it->second)
        {
            auto value{ StringUtils::readAs<T>(param) };
            if (value)
                res.emplace_back(std::move(*value));
        }

        return res;
    }

    template<typename T>
    std::vector<T> getMandatoryMultiParametersAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
    {
        std::vector<T> res{ getMultiParametersAs<T>(parameterMap, param) };
        if (res.empty())
            throw RequiredParameterMissingError{ param };

        return res;
    }

    template<typename T>
    std::optional<T> getParameterAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
    {
        std::vector<T> params{ getMultiParametersAs<T>(parameterMap, param) };

        if (params.size() != 1)
            return std::nullopt;

        return T{ std::move(params.front()) };
    }

    template<typename T>
    T getMandatoryParameterAs(const Wt::Http::ParameterMap& parameterMap, const std::string& param)
    {
        auto res{ getParameterAs<T>(parameterMap, param) };
        if (!res)
            throw RequiredParameterMissingError{ param };

        return *res;
    }

    bool hasParameter(const Wt::Http::ParameterMap& parameterMap, const std::string& param);
    std::string decodePasswordIfNeeded(const std::string& password);
}

