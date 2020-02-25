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
#include <memory>
#include <unordered_set>

// Used to get config values from configuration files
class IConfig
{
	public:
		virtual ~IConfig() = default;

		// Default values are returned in case of setting not found
		virtual std::string	getString(const std::string& setting, const std::string& def = "", const std::unordered_set<std::string>& allowedValues = {}) = 0;
		virtual std::filesystem::path getPath(const std::string& setting, const std::filesystem::path& def = std::filesystem::path()) = 0;
		virtual unsigned long	getULong(const std::string& setting, unsigned long def = 0) = 0;
		virtual long		getLong(const std::string& setting, long def = 0) = 0;
		virtual bool		getBool(const std::string& setting, bool def = false) = 0;
};


std::unique_ptr<IConfig> createConfig(const std::filesystem::path& p);

