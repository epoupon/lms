#ifndef CONFIG_READER_HPP
#define CONFIG_READER_HPP

#include <boost/filesystem.hpp>
#include <libconfig.h++>

#include "logger/Logger.hpp"

#include "service/UserInterfaceService.hpp"
#include "service/RemoteServerService.hpp"
#include "service/DatabaseUpdateService.hpp"

class ConfigReader
{
	public:

		ConfigReader(boost::filesystem::path p);

		// Logger configuration
		void getLoggerConfig(Logger::Config& config);

		// Service configurations
		void getUserInterfaceConfig(Service::UserInterfaceService::Config& config);
		void getRemoteServerConfig(Service::RemoteServerService::Config& config);
		void getDatabaseUpdateConfig(Service::DatabaseUpdateService::Config& config);

	private:

		libconfig::Config	_config;
};

#endif
