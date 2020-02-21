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

#include "utils/IConfig.hpp"

#include <libconfig.h++>

// Used to get config values from configuration files
class Config final : public IConfig
{
	public:
		Config(const std::filesystem::path& p);
		~Config() = default;

		Config(const Config&) = delete;
		Config& operator=(const Config&) = delete;
		Config(Config&&) = delete;
		Config& operator=(Config&&) = delete;

		// Default values are returned in case of setting not found
		std::string	getString(const std::string& setting, const std::string& def = "", const std::unordered_set<std::string>& allowedValues = {}) override;
		std::filesystem::path getPath(const std::string& setting, const std::filesystem::path& def = std::filesystem::path()) override;
		unsigned long	getULong(const std::string& setting, unsigned long def = 0) override;
		long		getLong(const std::string& setting, long def = 0) override;
		bool		getBool(const std::string& setting, bool def = false) override;

	private:

		libconfig::Config _config;
};

