/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <filesystem>
#include <functional>
#include <string_view>

namespace lms::core
{
    // Used to get config values from configuration files
    class IConfig
    {
    public:
        virtual ~IConfig() = default;

        // Default values are returned in case of setting not found
        virtual std::string_view getString(std::string_view setting, std::string_view def) = 0;
        virtual void visitStrings(std::string_view setting, std::function<void(std::string_view)> _func, std::initializer_list<std::string_view> def) = 0;
        virtual std::filesystem::path getPath(std::string_view setting, const std::filesystem::path& def) = 0;
        virtual unsigned long getULong(std::string_view setting, unsigned long def) = 0;
        virtual long getLong(std::string_view setting, long def) = 0;
        virtual bool getBool(std::string_view setting, bool def) = 0;
    };

    std::unique_ptr<IConfig> createConfig(const std::filesystem::path& p);
} // namespace lms::core