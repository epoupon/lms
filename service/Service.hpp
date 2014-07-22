#ifndef SERVICE_HPP
#define SERVICE_HPP

#include <boost/thread.hpp>
#include <memory>
#include <set>

// Interface class wrapper for running services
class Service
{
	public:

		typedef std::shared_ptr<Service>	pointer;

		Service(const Service&) = delete;
		Service& operator=(const Service&) = delete;

		Service() {}
		virtual ~Service() {}

		virtual void start(void) = 0;
		virtual void stop(void) = 0;
		virtual void restart(void) = 0;

};

#endif

