#include <iostream>
#include <iomanip>

#include "WebServerService.hpp"
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

WebServerService::WebServerService( int argc, char* argv[])
: _server(argv[0], "")
{

	std::cout << "Constructing server... argv[0] = '" << argv[0] << "' argc = " << argc << std::endl;

	// TODO configure server
	_server.setServerConfiguration (argc, argv, WTHTTP_CONFIGURATION);

	// bind entry point

	_server.addEntryPoint(Wt::Application, createApplication);

	std::cout << "WebServerService done!" << std::endl;
}

void
WebServerService::start(void)
{
	std::cout << "Starting service..." << std::endl;

	bool res = _server.start();
	std::cout << "_server.start returned " << std::boolalpha << res << std::endl;

	std::cout << "Service started..." << std::endl;
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


