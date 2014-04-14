#ifndef SERVICE_CONTROLER_HPP
#define SERVICE_CONTROLER_HPP

#include <boost/asio.hpp>
#include <set>

#include "Service.hpp"

// Start/Stop/Reload Services
class ServiceManager
{
	public:

		ServiceManager();

		void stopService(Service::pointer service);
		void startService(Service::pointer service);

		// Return in case of failure/stop by user
		void run();

		boost::asio::io_service&	getIoService() {return _ioService;}
		const boost::asio::io_service&	getIoService() const {return _ioService;}

	private:

		void restartServices(void);
		void stopServices(void);

		void asyncWaitSignals(void);

		void handleSignal(boost::system::error_code error, int signo);

		boost::asio::io_service	_ioService;

		// Listen for interesting signals
		boost::asio::signal_set	_signalSet;

		std::set<Service::pointer>	_services;
};


#endif

