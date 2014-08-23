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

		std::cout << "Starting services..." << std::endl;

		if (dbUpdateConfig.enable)
			serviceManager.startService( std::make_shared<Service::DatabaseUpdateService>( dbUpdateConfig ) );

		if (remoteConfig.enable)
			serviceManager.startService( std::make_shared<Service::RemoteServerService>( remoteConfig ));

		if (uiConfig.enable)
			serviceManager.startService( std::make_shared<Service::UserInterfaceService>(boost::filesystem::path(argv[0]), uiConfig));

		std::cout << "Running..." << std::endl;

		serviceManager.run();

		res = EXIT_SUCCESS;

	}
	catch( libconfig::ParseException& e)
	{
		std::cerr << "Caught libconfig::ParseException! error='" << e.getError() << "', file = '" << e.getFile() << "', line = " << e.getLine() << std::endl;
	}
	catch( Wt::WServer::Exception& e)
	{
		std::cerr << "Caught WServer::Exception: " << e.what() << std::endl;
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught std::exception: " << e.what() << std::endl;
	}

	return res;
}

