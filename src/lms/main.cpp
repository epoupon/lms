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

#include <thread>

#include <boost/property_tree/xml_parser.hpp>

#include <Wt/WServer.h>
#include <Wt/WApplication.h>

#include "auth/IAuthTokenService.hpp"
#include "auth/IPasswordService.hpp"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "cover/ICoverArtGrabber.hpp"
#include "database/Db.hpp"
#include "database/Session.hpp"
#include "scanner/IMediaScanner.hpp"
#include "recommendation/IEngine.hpp"
#include "subsonic/SubsonicResource.hpp"
#include "ui/LmsApplication.hpp"
#include "utils/IConfig.hpp"
#include "utils/Service.hpp"
#include "utils/WtLogger.hpp"

static
std::vector<std::string>
generateWtConfig(std::string execPath)
{
	std::vector<std::string> args;

	const std::filesystem::path wtConfigPath {Service<IConfig>::get()->getPath("working-dir") / "wt_config.xml"};
	const std::filesystem::path wtLogFilePath {Service<IConfig>::get()->getPath("log-file", "/var/log/lms.log")};
	const std::filesystem::path wtAccessLogFilePath {Service<IConfig>::get()->getPath("access-log-file", "/var/log/lms.access.log")};
	const std::filesystem::path wtResourcesPath {Service<IConfig>::get()->getPath("wt-resources", "/usr/share/Wt/resources")};
	const unsigned long configHttpServerThreadCount {Service<IConfig>::get()->getULong("http-server-thread-count", 0)};

	args.push_back(execPath);
	args.push_back("--config=" + wtConfigPath.string());
	args.push_back("--docroot=" + Service<IConfig>::get()->getString("docroot"));
	args.push_back("--approot=" + Service<IConfig>::get()->getString("approot"));
	args.push_back("--deploy-path=" + Service<IConfig>::get()->getString("deploy-path", "/"));
	if (!wtResourcesPath.empty())
		args.push_back("--resources-dir=" + wtResourcesPath.string());

	if (Service<IConfig>::get()->getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( Service<IConfig>::get()->getULong("listen-port", 5082)));
		args.push_back("--https-address=" + Service<IConfig>::get()->getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + Service<IConfig>::get()->getString("tls-cert"));
		args.push_back("--ssl-private-key=" + Service<IConfig>::get()->getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + Service<IConfig>::get()->getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( Service<IConfig>::get()->getULong("listen-port", 5082)));
		args.push_back("--http-address=" + Service<IConfig>::get()->getString("listen-addr", "0.0.0.0"));
	}

	if (!wtAccessLogFilePath.empty())
		args.push_back("--accesslog=" + wtAccessLogFilePath.string());

	{
		const unsigned long httpServerThreadCount {configHttpServerThreadCount ? configHttpServerThreadCount : std::max<unsigned long>(1, std::thread::hardware_concurrency())};
		args.push_back("--threads=" + std::to_string(httpServerThreadCount));
	}

	// Generate the wt_config.xml file
	boost::property_tree::ptree pt;

	pt.put("server.application-settings.<xmlattr>.location", "*");
	pt.put("server.application-settings.log-file", wtLogFilePath.string());
	pt.put("server.application-settings.log-config", Service<IConfig>::get()->getString("log-config", "* -debug -info:WebRequest"));
	pt.put("server.application-settings.behind-reverse-proxy", Service<IConfig>::get()->getBool("behind-reverse-proxy", false));

	{
		boost::property_tree::ptree viewport;
		viewport.put("<xmlattr>.name", "viewport");
		viewport.put("<xmlattr>.content", "width=device-width, initial-scale=1, user-scalable=no");
		pt.add_child("server.application-settings.head-matter.meta", viewport);
	}
	{
		boost::property_tree::ptree themeColor;
		themeColor.put("<xmlattr>.name", "theme-color");
		themeColor.put("<xmlattr>.content", "#303030");
		pt.add_child("server.application-settings.head-matter.meta", themeColor);
	}


	{
		std::ofstream oss {wtConfigPath.string().c_str(), std::ios::out};
		if (!oss)
			throw LmsException {"Can't open '" + wtConfigPath.string() + "' for writing!"};

		boost::property_tree::xml_parser::write_xml(oss, pt);

		if (!oss)
			throw LmsException {"Can't write in file '" + wtConfigPath.string() + "', no space left?"};
	}

	return args;
}


int main(int argc, char* argv[])
{
	std::filesystem::path configFilePath {"/etc/lms.conf"};
	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	if (argc == 2)
		configFilePath = std::string(argv[1], 0, 256);
	else if (argc > 2)
	{
		std::cerr << "Usage:\t" << argv[0] << "\t[conf_file]\n\n"
			<< "Options:\n"
			<< "\tconf_file:\t path to the LMS configuration file (defaults to " << configFilePath << ")\n\n";
		return EXIT_FAILURE;
	}

	try
	{
		// Make pstream work with ffmpeg
		close(STDIN_FILENO);

		Service<IConfig> config {createConfig(configFilePath)};
		Service<Logger> logger {std::make_unique<WtLogger>()};

		// Make sure the working directory exists
		std::filesystem::create_directories(config->getPath("working-dir"));
		std::filesystem::create_directories(config->getPath("working-dir") / "cache");

		// Construct WT configuration and get the argc/argv back
		const std::vector<std::string> wtServerArgs {generateWtConfig(argv[0])};

		std::vector<const char*> wtArgv(wtServerArgs.size());
		for (std::size_t i = 0; i < wtServerArgs.size(); ++i)
		{
			std::cout << "ARG = " << wtServerArgs[i] << std::endl;
			wtArgv[i] = wtServerArgs[i].c_str();
		}

		Wt::WServer server {argv[0]};
		server.setServerConfiguration(wtServerArgs.size(), const_cast<char**>(&wtArgv[0]));

		// lib init
		Av::Transcoder::init();

		// Initializing a connection pool to the database that will be shared along services
		Database::Db database {config->getPath("working-dir") / "lms.db"};
		{
			Database::Session session {database};
			session.prepareTables();
			session.optimize();
		}

		UserInterface::LmsApplicationGroupContainer appGroups;

		// Service initialization order is important
		Service<Auth::IAuthTokenService> authTokenService {Auth::createAuthTokenService(config->getULong("login-throttler-max-entriees", 10000))};
		Service<Auth::IPasswordService> passwordService {Auth::createPasswordService(config->getULong("login-throttler-max-entriees", 10000))};
		Service<CoverArt::IGrabber> coverArtService {CoverArt::createGrabber(argv[0],
				server.appRoot() + "/images/unknown-cover.jpg",
				config->getULong("cover-max-cache-size", 30) * 1000 * 1000,
				config->getULong("cover-max-file-size", 10) * 1000 * 1000,
				config->getULong("cover-jpeg-quality", 75))};
		Service<Recommendation::IEngine> recommendationEngineService {Recommendation::createEngine(database)};
		recommendationEngineService->requestLoad();
		Service<Scanner::IMediaScanner> mediaScannerService {Scanner::createMediaScanner(database)};

		mediaScannerService->scanComplete().connect([&]()
		{
			auto status = mediaScannerService->getStatus();

			if (status.lastCompleteScanStats->nbChanges() > 0 || status.lastCompleteScanStats->featuresFetched > 0)
			{
				LMS_LOG(MAIN, INFO) << "Scanner changed some files, reloading the recommendation engine...";
				recommendationEngineService->requestReload();
			}
			else
			{
				LMS_LOG(MAIN, INFO) << "Scanner did not change files, not reloading the recommendation engine...";
			}
			// Flush cover cache even if no changes:
			// covers may be external files that changed and we don't keep track of them
			coverArtService->flushCache();
		});

		API::Subsonic::SubsonicResource subsonicResource {database};

		// bind API resources
		if (config->getBool("api-subsonic", true))
			server.addResource(&subsonicResource, subsonicResource.getPath());

		// bind UI entry point
		server.addEntryPoint(Wt::EntryPointType::Application,
				std::bind(UserInterface::LmsApplication::create,
					std::placeholders::_1, std::ref(database), std::ref(appGroups)));

		LMS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		LMS_LOG(MAIN, INFO) << "Now running...";
		Wt::WServer::waitForShutdown();

		LMS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		LMS_LOG(MAIN, INFO) << "Clean stop!";
		res = EXIT_SUCCESS;
	}
	catch(Wt::WServer::Exception& e)
	{
		std::cerr << "Caught a WServer::Exception: " << e.what() << std::endl;
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught std::exception: " << e.what() << std::endl;
	}

	return res;
}

