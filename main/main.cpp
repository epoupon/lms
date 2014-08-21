#include <boost/filesystem.hpp>

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

	try
	{
		// TODO generate a nice command line help with args

		// Open configuration file
		boost::filesystem::path configFile("/etc/lms.conf");
		if (argc > 1)
			configFile = boost::filesystem::path(argv[1]);

		if ( !boost::filesystem::exists(configFile)
			|| !boost::filesystem::is_regular(configFile))
		{
			std::cerr << "Cannot open config file '" << configFile << "'" << std::endl;
			return EXIT_FAILURE;
		}

		ConfigReader configReader(configFile);

		Service::UserInterfaceService::Config uiConfig;
		configReader.getUserInterfaceConfig(uiConfig);

		Service::ServiceManager& serviceManager = Service::ServiceManager::instance();

		// TODO Retreive the database path in some config file
		const boost::filesystem::path dbPath("test.db");
		Remote::Server::Server::endpoint_type remoteListenEndpoint( boost::asio::ip::address::from_string("0.0.0.0"), 5080);

		// lib init
		Av::AvInit();
		Transcode::AvConvTranscoder::init();
		Database::Handler::configureAuth();

		std::cout << "Starting services..." << std::endl;

		serviceManager.startService( std::make_shared<Service::DatabaseUpdateService>( dbPath) );
		serviceManager.startService( std::make_shared<Service::RemoteServerService>( remoteListenEndpoint, dbPath) );
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

