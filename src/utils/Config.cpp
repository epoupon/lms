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

#include "Config.hpp"

#include <sstream>

#include "utils/Logger.hpp"


Config&
Config::instance()
{
	static Config instance;
	return instance;
}

void
Config::setFile(const boost::filesystem::path& p)
{
	_config = std::make_unique<libconfig::Config>();
	_config->readFile(p.string().c_str());
}

std::string
Config::getString(const std::string& setting, const std::string& def, const std::set<std::string>& allowedValues)
{
	try {
		std::string res {(const char*)_config->lookup(setting)};

		if (!allowedValues.empty() && allowedValues.find(res) == allowedValues.end())
		{
			LMS_LOG(MAIN, ERROR) << "Invalid setting for '" << setting << "', using default value '" << def << "'";
			return def;
		}

		return res;
	}
	catch (std::exception &e)
	{
		return def;
	}
}

boost::filesystem::path
Config::getPath(const std::string& setting, const boost::filesystem::path& path)
{
	try {
		const char* res = _config->lookup(setting);
		return boost::filesystem::path(std::string(res));
	}
	catch (std::exception &e)
	{
		return path;
	}
}

unsigned long
Config::getULong(const std::string& setting, unsigned long def)
{
	try {
		return static_cast<unsigned int>(_config->lookup(setting));
	}
	catch (...)
	{
		return def;
	}
}

long
Config::getLong(const std::string& setting, long def)
{
	try {
		return _config->lookup(setting);
	}
	catch (...)
	{
		return def;
	}
}

bool
Config::getBool(const std::string& setting, bool def)
{
	try {
		return _config->lookup(setting);
	}
	catch (...)
	{
		return def;
	}
}


