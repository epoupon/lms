#ifndef WEB_SERVER_SERVICE_HPP
#define WEB_SERVER_SERVICE_HPP

#include <Wt/WServer>

#include "Service.hpp"

class WebServerService : public Service
{
	public:
		WebServerService( int argc, char* argv[]);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		Wt::WServer	_server;

};

#endif

