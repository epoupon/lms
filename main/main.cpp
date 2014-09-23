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

#include <boost/filesystem.hpp>

#include "logger/Logger.hpp"

#include "config/ConfigReader.hpp"
#include "transcode/AvConvTranscoder.hpp"
#include "av/Common.hpp"

#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"
#include "service/UserInterfaceService.hpp"
#include "service/RemoteServerService.hpp"

int main(int argc, char* argv[])
{

	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	try
	{
		// TODO generate a nice command line help with args

		// Open configuration file
		boost::filesystem::path configFile("/etc/lms.conf"); // TODO
		if (argc > 1)
			configFile = boost::filesystem::path(argv[1]);

		if ( !boost::filesystem::exists(configFile))
		{
			std::cerr << "Config file '" << configFile << "' does not exist!" << std::endl;
			return EXIT_FAILURE;
		}
		else if (!boost::filesystem::is_regular(configFile))
		{
			std::cerr << "Config file '" << configFile << "' is not regular!" << std::endl;
			return EXIT_FAILURE;
		}

		ConfigReader configReader(configFile);

		// Initializa logging facility
		{
			Logger::Config loggerConfig;
			configReader.getLoggerConfig(loggerConfig);
			Logger::instance().init(loggerConfig);
		}

		LMS_LOG(MOD_MAIN, SEV_INFO) << "Reading service configurations...";

		Service::DatabaseUpdateService::Config dbUpdateConfig;
		configReader.getDatabaseUpdateConfig(dbUpdateConfig);

		Service::UserInterfaceService::Config uiConfig;
		configReader.getUserInterfaceConfig(uiConfig);

		Service::RemoteServerService::Config remoteConfig;
		configReader.getRemoteServerConfig(remoteConfig);

		Service::ServiceManager& serviceManager = Service::ServiceManager::instance();

		// lib init
		Av::AvInit();
		Transcode::AvConvTranscoder::init();
		Database::Handler::configureAuth();

		LMS_LOG(MOD_MAIN, SEV_INFO) << "Starting services...";

		if (dbUpdateConfig.enable)
			serviceManager.startService( std::make_shared<Service::DatabaseUpdateService>( dbUpdateConfig ) );

		if (remoteConfig.enable)
			serviceManager.startService( std::make_shared<Service::RemoteServerService>( remoteConfig ));

		if (uiConfig.enable)
			serviceManager.startService( std::make_shared<Service::UserInterfaceService>(boost::filesystem::path(argv[0]), uiConfig));

		LMS_LOG(MOD_MAIN, SEV_NOTICE) << "Now running...";

		serviceManager.run();

		res = EXIT_SUCCESS;

	}
	catch( libconfig::ParseException& e)
	{
		std::cerr << "Caught libconfig::ParseException! error='" << e.getError() << "', file = '" << e.getFile() << "', line = " << e.getLine() << std::endl;
	}
	catch( Wt::WServer::Exception& e)
	{
		LMS_LOG(MOD_MAIN, SEV_CRIT) << "Caught a WServer::Exception: " << e.what();
	}
	catch( std::exception& e)
	{
		LMS_LOG(MOD_MAIN, SEV_CRIT) << "Caught std::exception: " << e.what();
	}

	return res;
}

