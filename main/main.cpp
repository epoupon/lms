#include <memory>
#include <Wt/WServer>

#include "transcode/AvConvTranscoder.hpp"
#include "av/Common.hpp"

#include "ServiceManager.hpp"
#include "DatabaseRefreshService.hpp"
#include "WebServerService.hpp"
#include "RemoteServerService.hpp"

#include "ui/LmsApplication.hpp"

static Wt::WApplication *createApplication(const Wt::WEnvironment& env)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	std::cout << "Creating new Application" << std::endl;
	  return new LmsApplication(env);
}

int main(int argc, char* argv[])
{

	int res = EXIT_FAILURE;

	try
	{
//		ServiceManager serviceManager;

		// lib init
		Av::AvInit();
		Transcode::AvConvTranscoder::init();

		return Wt::WRun(argc, argv, createApplication);
/*		Wt::WServer server(argv[0], "");

		server.setServerConfiguration(argc, argv, WTHTTP_CONFIGURATION);
		server.addEntryPoint(Wt::Application, createApplication);
		if (server.start()) {
			int sig = Wt::WServer::waitForShutdown(argv[0]);
			std::cerr << "Shutdown (signal = " << sig << ")" << std::endl;
			server.stop();
			if (sig == SIGHUP)
				Wt::WServer::restart(argc, argv, environ);
		}*/
//		return 0;


//		serviceManager.startService( std::make_shared<DatabaseRefreshService>( "test.db" ) );
//		serviceManager.startService( std::make_shared<WebServerService>(argc, argv) );
//		serviceManager.startService( std::make_shared<RemoteServerService>( Remote::Server::Server::endpoint_type() ) );

//		serviceManager.run();

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

