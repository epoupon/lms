#include <iostream>
#include <iomanip>

#include "UserInterfaceService.hpp"
#include "ui/LmsApplication.hpp"

UserInterfaceService::UserInterfaceService( int argc, char* argv[], boost::filesystem::path dbPath)
: _server(argv[0], "")
{
	// TODO configure server using another way (config file?)
	_server.setServerConfiguration (argc, argv, WTHTTP_CONFIGURATION);

	// bind entry point
	_server.addEntryPoint(Wt::Application, boost::bind(UserInterface::LmsApplication::create, _1, dbPath));
}

void
UserInterfaceService::start(void)
{
	_server.start();
	std::cout << "UserInterfaceService::start -> Service started..." << std::endl;
}

void
UserInterfaceService::stop(void)
{
	std::cout << "UserInterfaceService::stop -> stopping..." << std::endl;
	_server.stop();
	std::cout << "UserInterfaceService::stop -> stopped!" << std::endl;
}

void
UserInterfaceService::restart(void)
{

}


