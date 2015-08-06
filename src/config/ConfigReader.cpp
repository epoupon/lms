/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <sstream>

#include "ConfigReader.hpp"

namespace {

}

ConfigReader::ConfigReader()
: _config (nullptr)
{
}

ConfigReader&
ConfigReader::instance()
{
	static ConfigReader instance;
	return instance;
}

void
ConfigReader::setFile(boost::filesystem::path p)
{
	if (_config != nullptr)
		delete _config;

	_config = new libconfig::Config();

	_config->readFile(p.string().c_str());
}

std::string
ConfigReader::getString(std::string setting)
{
	return _config->lookup(setting);
}

unsigned long
ConfigReader::getULong(std::string setting)
{
	return static_cast<unsigned int>(_config->lookup(setting));
}

long
ConfigReader::getLong(std::string setting)
{
	return _config->lookup(setting);
}

bool
ConfigReader::getBool(std::string setting)
{
	return _config->lookup(setting);
}

