/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sstream>

#include "ConfigReader.hpp"

namespace {

	void splitStrings(const std::string& source, std::vector<std::string>& res)
	{
		std::istringstream oss(source);

		std::string str;
		while(oss >> str)
			res.push_back(str);
	}

}

ConfigReader::ConfigReader(boost::filesystem::path p)
{
	_config.readFile(p.string().c_str());
}

void
ConfigReader::getLoggerConfig(Logger::Config& config)
{
	config.enableFileLogging = _config.lookupValue("main.logger.file", config.logPath);
	config.enableConsoleLogging = _config.lookup("main.logger.console");
	config.minSeverity = static_cast<Severity>((int)_config.lookup("main.logger.level"));
}

void
ConfigReader::getCoverGrabberConfig(CoverArt::Grabber::Config& config)
{
	std::string extensions = _config.lookup("main.cover.file_extensions");
	config.maxFileSize = static_cast<unsigned int>(_config.lookup("main.cover.file_max_size"));
	std::string filenames = _config.lookup("main.cover.file_preferred_names");

	splitStrings(extensions, config.fileExtensions);
	splitStrings(filenames, config.preferredFileNames);
}

void
ConfigReader::getUserInterfaceConfig(Service::UserInterfaceService::Config& config)
{

	config.enable = _config.lookup("ui.enable");
	if (!config.enable)
		return;

	config.docRootPath = _config.lookup("ui.resources.docroot");
	config.appRootPath = _config.lookup("ui.resources.approot");
	config.httpsPort = static_cast<unsigned int>(_config.lookup("ui.listen-endpoint.port"));
	config.httpsAddress = boost::asio::ip::address::from_string((const char*)_config.lookup("ui.listen-endpoint.addr"));
	config.sslCertificatePath = _config.lookup("ui.ssl-crypto.cert");
	config.sslPrivateKeyPath = _config.lookup("ui.ssl-crypto.key");
	config.sslTempDhPath = _config.lookup("ui.ssl-crypto.dh");

	config.dbPath = _config.lookup("main.database.path");
}

#if defined HAVE_LMSAPI
void
ConfigReader::getLmsAPIConfig(Service::LmsAPIService::Config& config)
{
	config.enable = _config.lookup("remote.enable");
	if (!config.enable)
		return;

	config.port = static_cast<unsigned int>(_config.lookup("remote.listen-endpoint.port"));
	config.address = boost::asio::ip::address::from_string((const char*)_config.lookup("remote.listen-endpoint.addr"));
	config.sslCertificatePath = _config.lookup("remote.ssl-crypto.cert");
	config.sslPrivateKeyPath = _config.lookup("remote.ssl-crypto.key");
	config.sslTempDhPath = _config.lookup("remote.ssl-crypto.dh");

	config.dbPath = _config.lookup("main.database.path");
}
#endif

void
ConfigReader::getDatabaseUpdateConfig(Service::DatabaseUpdateService::Config& config)
{
	config.enable = true;

	config.dbPath = _config.lookup("main.database.path");

	std::string audioExtensions = _config.lookup("main.database.audio_extensions");
	std::string videoExtensions = _config.lookup("main.database.video_extensions");

	splitStrings(audioExtensions, config.audioExtensions);
	splitStrings(videoExtensions, config.videoExtensions);
}

