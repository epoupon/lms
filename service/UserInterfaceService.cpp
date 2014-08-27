#include "logger/Logger.hpp"

#include "UserInterfaceService.hpp"
#include "ui/LmsApplication.hpp"

namespace Service {

UserInterfaceService::UserInterfaceService( boost::filesystem::path runAppPath, const Config& config)
: _server(runAppPath.string())
{
	std::vector<std::string> args;

	args.push_back(runAppPath.string());
	args.push_back("--docroot=" + config.docRootPath.string());
	args.push_back("--approot=" + config.appRootPath.string());
	{
		std::ostringstream oss; oss << config.httpsPort;
		args.push_back("--https-port=" + oss.str());
	}
	args.push_back("--https-address=" + config.httpsAddress.to_string());
	args.push_back("--ssl-certificate=" + config.sslCertificatePath.string());
	args.push_back("--ssl-private-key=" + config.sslPrivateKeyPath.string());
	args.push_back("--ssl-tmp-dh=" + config.sslTempDhPath.string());

	// Construct argc/argv
	int argc = args.size();
	const char* argv[args.size()];
	for (int i = 0; i < argc; ++i)
		argv[i] = args[i].c_str();

	for(int i = 0; i < argc; ++i)
	{
		LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "i = " << i << ", arg = '" << argv[i] << "'";
	}

	_server.setServerConfiguration (argc, const_cast<char**>(argv));

	// bind entry point
	_server.addEntryPoint(Wt::Application, boost::bind(UserInterface::LmsApplication::create, _1, config.dbPath));

}

void
UserInterfaceService::start(void)
{
	_server.start();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "UserInterfaceService::start -> Service started...";
}

void
UserInterfaceService::stop(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "UserInterfaceService::stop -> stopping...";
	_server.stop();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "UserInterfaceService::stop -> stopped!";
}

void
UserInterfaceService::restart(void)
{

}

} // namespace Service

