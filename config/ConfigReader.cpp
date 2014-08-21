#include "ConfigReader.hpp"

ConfigReader::ConfigReader(boost::filesystem::path p)
{
	_config.readFile(p.string().c_str());
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

