
#include "WebServerService.hpp"
#include "ui/LmsApplication.hpp"


Wt::WApplication *createApplication(const Wt::WEnvironment& env)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	  return new LmsApplication(env);
}


WebServerService::WebServerService( int argc, char* argv[])
: _server(argv[0])
{

	// TODO configure server
	_server.setServerConfiguration (argc, argv, WTHTTP_CONFIGURATION);

	// bind entry point

	_server.addEntryPoint(Wt::Application, createApplication);

}

void
WebServerService::start(void)
{
	 _server.start();
}

void
WebServerService::stop(void)
{
	 _server.stop();
}

void
WebServerService::restart(void)
{

}


