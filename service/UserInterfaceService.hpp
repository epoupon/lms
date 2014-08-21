#ifndef WEB_SERVER_SERVICE_HPP
#define WEB_SERVER_SERVICE_HPP

#include <boost/filesystem.hpp>
#include <boost/asio/ip/address.hpp>

#include <Wt/WServer>

#include "Service.hpp"

namespace Service {

class UserInterfaceService : public Service
{
	public:

		struct Config {
			bool 			enable;
			boost::filesystem::path docRootPath;
			boost::filesystem::path appRootPath;
			unsigned short		httpsPort;
			boost::asio::ip::address	httpsAddress;
			boost::filesystem::path sslCertificatePath;
			boost::filesystem::path sslPrivateKeyPath;
			boost::filesystem::path sslTempDhPath;
			boost::filesystem::path dbPath;
		};

		UserInterfaceService(boost::filesystem::path runAppPath,
					const Config& config);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		Wt::WServer	_server;

};

} //namespace Service

#endif

