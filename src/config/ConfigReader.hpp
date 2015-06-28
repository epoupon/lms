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

#ifndef CONFIG_READER_HPP
#define CONFIG_READER_HPP

#include <boost/filesystem.hpp>
#include <libconfig.h++>

#include "cover/CoverArtGrabber.hpp"
#include "logger/Logger.hpp"

#include "service/UserInterfaceService.hpp"
#include "service/RemoteServerService.hpp"
#include "service/DatabaseUpdateService.hpp"

class ConfigReader
{
	public:

		ConfigReader(boost::filesystem::path p);

		// Logger configuration
		void getLoggerConfig(Logger::Config& config);

		// Covers
		void getCoverGrabberConfig(CoverArt::Grabber::Config& config);

		// Service configurations
		void getUserInterfaceConfig(Service::UserInterfaceService::Config& config);
		void getRemoteServerConfig(Service::RemoteServerService::Config& config);
		void getDatabaseUpdateConfig(Service::DatabaseUpdateService::Config& config);

	private:

		libconfig::Config	_config;
};

#endif
