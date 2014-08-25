#include "ConfigReader.hpp"

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

	config.dbPath = _config.lookup("main.db");
}

void
ConfigReader::getRemoteServerConfig(Service::RemoteServerService::Config& config)
{
	config.enable = _config.lookup("remote.enable");
	if (!config.enable)
		return;

	config.port = static_cast<unsigned int>(_config.lookup("remote.listen-endpoint.port"));
	config.address = boost::asio::ip::address::from_string((const char*)_config.lookup("remote.listen-endpoint.addr"));
	config.sslCertificatePath = _config.lookup("remote.ssl-crypto.cert");
	config.sslPrivateKeyPath = _config.lookup("remote.ssl-crypto.key");
	config.sslTempDhPath = _config.lookup("remote.ssl-crypto.dh");

	config.dbPath = _config.lookup("main.db");
}

void
ConfigReader::getDatabaseUpdateConfig(Service::DatabaseUpdateService::Config& config)
{
	config.enable = true;

	config.dbPath = _config.lookup("main.db");
}

