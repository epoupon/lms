#ifndef CONFIG_READER_HPP
#define CONFIG_READER_HPP

#include <boost/filesystem.hpp>
#include <libconfig.h++>

#include "service/UserInterfaceService.hpp"

class ConfigReader
{
	public:

		ConfigReader(boost::filesystem::path p);

		void getUserInterfaceConfig(Service::UserInterfaceService::Config& config);


	private:

		libconfig::Config	_config;
};

#endif
