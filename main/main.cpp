#include <memory>
#include <Wt/WServer>

#include "transcode/AvConvTranscoder.hpp"
#include "av/Common.hpp"

#include "ServiceManager.hpp"
#include "DatabaseRefreshService.hpp"
#include "WebServerService.hpp"
#include "RemoteServerService.hpp"

void toto(int argc, char* argv[]);

int main(int argc, char* argv[])
{

	int res = EXIT_FAILURE;

	try
	{
		ServiceManager serviceManager;

		// lib init
		Av::AvInit();
		Transcode::AvConvTranscoder::init();

//		serviceManager.startService( std::make_shared<DatabaseRefreshService>( "test.db" ) );
		serviceManager.startService( std::make_shared<WebServerService>(argc, argv) );
//		serviceManager.startService( std::make_shared<RemoteServerService>( Remote::Server::Server::endpoint_type() ) );

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

