#include "logger/Logger.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#include "ServiceManager.hpp"

namespace Service {

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

	// Excplicitely ignore SIGCHLD to avoid zombies
	// when avconv child processes are being killed
	if (::signal(SIGCHLD, SIG_IGN) == SIG_ERR)
		throw std::runtime_error("ServiceManager::ServiceManager, signal failed!");
}

ServiceManager::~ServiceManager()
{
}

void
ServiceManager::run()
{

	asyncWaitSignals();

	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "ServiceManager: waiting for events...";
	try {
		// Wait for events
		_ioService.run();
	}
	catch( std::exception& e )
	{
		LMS_LOG(MOD_SERVICE, SEV_ERROR) << "ServiceManager: exception in ioService::run: " << e.what();
	}

	// Stopping services
	stopServices();

	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "ServiceManager: run complete !";
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
	_services.insert(service);
	service->start();
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
	LMS_LOG(MOD_SERVICE, SEV_INFO) << "Received signal " << signo;

	switch (signo)
	{
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			LMS_LOG(MOD_SERVICE, SEV_NOTICE) << "Stopping services...";
			stopServices();

			// Do not listen for signals, this will make the ioservice.run return
			break;
		case SIGHUP:
			LMS_LOG(MOD_SERVICE, SEV_NOTICE) << "Restarting services...";

			restartServices();
			asyncWaitSignals();
			break;
		default:
			assert(0);
	}
}

} // namespace Service

