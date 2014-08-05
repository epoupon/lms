
//#include <csignal>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#include "ServiceManager.hpp"

ServiceManager&
ServiceManager::instance()
{
	static ServiceManager instance;
	return instance;
}

ServiceManager::ServiceManager()
: _signalSet(_ioService)
{
	_signalSet.add(SIGINT);
	_signalSet.add(SIGTERM);
#if defined(SIGQUIT)
	_signalSet.add(SIGQUIT);
#endif // defined(SIGQUIT)

	_signalSet.add(SIGHUP);
}

ServiceManager::~ServiceManager()
{
}

void
ServiceManager::run()
{

	asyncWaitSignals();

	std::cout << "ServiceManager::run Waiting for events..." << std::endl;
	try {
		// Wait for events
		_ioService.run();
	}
	catch( std::exception& e )
	{
		std::cerr << "Caugh exception in service : " << e.what() << std::endl;
	}

	// Stopping services
	stopServices();

	std::cout << "ServiceManager::run complete!" << std::endl;
}

void
ServiceManager::asyncWaitSignals(void)
{
	_signalSet.async_wait(boost::bind(&ServiceManager::handleSignal,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::signal_number));
}

void
ServiceManager::startService(Service::pointer service)
{
	std::cout << "ServiceManager::startService" << std::endl;
	_services.insert(service);
	service->start();
	std::cout << "ServiceManager::startService done" << std::endl;
}

void
ServiceManager::stopService(Service::pointer service)
{
	_services.erase(service);
	service->stop();
}

void
ServiceManager::stopServices(void)
{
	BOOST_FOREACH(Service::pointer service, _services)
		service->stop();
}


void
ServiceManager::restartServices(void)
{
	BOOST_FOREACH(Service::pointer service, _services)
		service->restart();
}

void
ServiceManager::handleSignal(boost::system::error_code /*ec*/, int signo)
{
	std::cout << "Received signal " << signo << std::endl;

	switch (signo)
	{
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			std::cout << "Stopping services..." << std::endl;
			stopServices();

			// Do not listen for signals, this will make the ioservice.run return
			break;
		case SIGHUP:
			std::cout << "Restarting services..." << std::endl;

			restartServices();
			asyncWaitSignals();
			break;
		default:
			assert(0);
	}
}

