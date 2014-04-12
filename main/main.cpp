#include <memory>
#include <Wt/WServer>
#include <Wt/WIOService>
#include <csignal>

#include "transcode/AvConvTranscoder.hpp"
#include "av/Common.hpp"

#include "ServiceManager.hpp"
#include "DatabaseRefreshService.hpp"
#include "UserInterfaceService.hpp"
#include "RemoteServerService.hpp"

#include "ui/LmsApplication.hpp"

int main(int argc, char* argv[])
{

	int res = EXIT_FAILURE;

	try
	{
		ServiceManager serviceManager;

		// TODO Retreive the database path in some config file
		const boost::filesystem::path dbPath("test.db");

		// lib init
		Av::AvInit();
		Transcode::AvConvTranscoder::init();

		serviceManager.startService( std::make_shared<DatabaseRefreshService>( dbPath) );
		serviceManager.startService( std::make_shared<UserInterfaceService>(argc, argv, dbPath) );
		serviceManager.startService( std::make_shared<RemoteServerService>( Remote::Server::Server::endpoint_type(), dbPath) );

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

