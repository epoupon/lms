#ifndef SERVICE_CONTROLER_HPP
#define SERVICE_CONTROLER_HPP

#include <boost/asio.hpp>
#include <set>

#include "Service.hpp"

// Start/Stop/Reload Services
class ServiceManager
{
	public:

		static ServiceManager& instance();
		~ServiceManager();

		void stopService(Service::pointer service);
		void startService(Service::pointer service);

		void stopAllServices();

		// Return in case of failure/stop by user
		void run();

		template <class T> typename T::pointer getService();

		boost::mutex&	mutex() { return _mutex;}

	private:

		ServiceManager();
		ServiceManager(ServiceManager const&);	// Don't Implement
		void operator=(ServiceManager const&);	// Don't implement

		void restartServices(void);
		void stopServices(void);

		void asyncWaitSignals(void);

		void handleSignal(boost::system::error_code error, int signo);

		boost::mutex	_mutex;

		boost::asio::io_service	_ioService;

		// Listen for interesting signals
		boost::asio::signal_set	_signalSet;

		std::set<Service::pointer>	_services;
};


template <class T> typename T::pointer
ServiceManager::getService()
{
	std::set<Service::pointer>::iterator it;
	for (std::set<Service::pointer>::iterator it = _services.begin(); it != _services.end(); ++it)
	{
		if (typeid(*(*it)) == typeid(T)) {
			return std::dynamic_pointer_cast<T>(*it);
		}
	}
	return std::shared_ptr<T>();
}

#endif

