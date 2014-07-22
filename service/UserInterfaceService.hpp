#ifndef WEB_SERVER_SERVICE_HPP
#define WEB_SERVER_SERVICE_HPP

#include <boost/filesystem.hpp>

#include <Wt/WServer>

#include "Service.hpp"

class UserInterfaceService : public Service
{
	public:
		UserInterfaceService( int argc, char* argv[], boost::filesystem::path dbPath);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		Wt::WServer	_server;

};

#endif

