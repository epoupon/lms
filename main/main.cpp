#include <memory>
#include <Wt/WServer>
#include <Wt/WIOService>
#include <csignal>

#include "transcode/AvConvTranscoder.hpp"
#include "av/Common.hpp"
#include "database/DatabaseHandler.hpp"

#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"
#include "service/UserInterfaceService.hpp"
#include "service/RemoteServerService.hpp"

#include "ui/LmsApplication.hpp"

int main(int argc, char* argv[])
{

	int res = EXIT_FAILURE;

	try
	{
		ServiceManager& serviceManager = ServiceManager::instance();

		// TODO Retreive the database path in some config file
		const boost::filesystem::path dbPath("test.db");
		Remote::Server::Server::endpoint_type remoteListenEndpoint( boost::asio::ip::address::from_string("0.0.0.0"), 5080);

		// lib init
		Av::AvInit();
		Transcode::AvConvTranscoder::init();
		Database::Handler::configureAuth();

		std::cout << "Starting services..." << std::endl;

		serviceManager.startService( std::make_shared<DatabaseUpdateService>( dbPath) );
		serviceManager.startService( std::make_shared<RemoteServerService>( remoteListenEndpoint, dbPath) );
		serviceManager.startService( std::make_shared<UserInterfaceService>(argc, argv, dbPath) );

		std::cout << "Running..." << std::endl;

		serviceManager.run();

		res = EXIT_SUCCESS;

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

