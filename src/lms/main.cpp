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

#include <boost/property_tree/xml_parser.hpp>

#include <Wt/WServer.h>
#include <Wt/WApplication.h>

#include "auth/IAuthTokenService.hpp"
#include "auth/IPasswordService.hpp"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "cover/ICoverArtGrabber.hpp"
#include "database/Db.hpp"
#include "scanner/IMediaScanner.hpp"
#include "recommendation/IEngine.hpp"
#include "subsonic/SubsonicResource.hpp"
#include "ui/LmsApplication.hpp"
#include "utils/IConfig.hpp"
#include "utils/Service.hpp"
#include "utils/WtLogger.hpp"

std::vector<std::string> generateWtConfig(std::string execPath)
{
	std::vector<std::string> args;

	const std::filesystem::path wtConfigPath {ServiceProvider<IConfig>::get()->getPath("working-dir") / "wt_config.xml"};
	const std::filesystem::path wtLogFilePath {ServiceProvider<IConfig>::get()->getPath("log-file", "/var/log/lms.log")};
	const std::filesystem::path wtAccessLogFilePath {ServiceProvider<IConfig>::get()->getPath("access-log-file", "/var/log/lms.access.log")};

	args.push_back(execPath);
	args.push_back("--config=" + wtConfigPath.string());
	args.push_back("--docroot=" + ServiceProvider<IConfig>::get()->getString("docroot"));
	args.push_back("--approot=" + ServiceProvider<IConfig>::get()->getString("approot"));
	args.push_back("--deploy-path=" + ServiceProvider<IConfig>::get()->getString("deploy-path", "/"));
	args.push_back("--resources-dir=" + ServiceProvider<IConfig>::get()->getString("wt-resources"));

	if (ServiceProvider<IConfig>::get()->getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( ServiceProvider<IConfig>::get()->getULong("listen-port", 5082)));
		args.push_back("--https-address=" + ServiceProvider<IConfig>::get()->getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + ServiceProvider<IConfig>::get()->getString("tls-cert"));
		args.push_back("--ssl-private-key=" + ServiceProvider<IConfig>::get()->getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + ServiceProvider<IConfig>::get()->getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( ServiceProvider<IConfig>::get()->getULong("listen-port", 5082)));
		args.push_back("--http-address=" + ServiceProvider<IConfig>::get()->getString("listen-addr", "0.0.0.0"));
	}

	if (!wtAccessLogFilePath.empty())
		args.push_back("--accesslog=" + wtAccessLogFilePath.string());

	// Generate the wt_config.xml file
	boost::property_tree::ptree pt;

	pt.put("server.application-settings.<xmlattr>.location", "*");
	pt.put("server.application-settings.log-file", wtLogFilePath.string());
	pt.put("server.application-settings.log-config", ServiceProvider<IConfig>::get()->getString("log-config", "* -debug -info:WebRequest"));
	pt.put("server.application-settings.behind-reverse-proxy", ServiceProvider<IConfig>::get()->getBool("behind-reverse-proxy", false));
	pt.put("server.application-settings.progressive-bootstrap", true);

	std::ofstream oss(wtConfigPath.string().c_str(), std::ios::out);
	boost::property_tree::xml_parser::write_xml(oss, pt);

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

		ServiceProvider<IConfig>::assign(createConfig(configFilePath));
		ServiceProvider<Logger>::create<WtLogger>();

		// Make sure the working directory exists
		std::filesystem::create_directories(ServiceProvider<IConfig>::get()->getPath("working-dir"));
		std::filesystem::create_directories(ServiceProvider<IConfig>::get()->getPath("working-dir") / "cache");

		// Construct WT configuration and get the argc/argv back
		std::vector<std::string> wtServerArgs = generateWtConfig(argv[0]);

		std::vector<const char*> wtArgv(wtServerArgs.size());
		for (std::size_t i = 0; i < wtServerArgs.size(); ++i)
		{
			std::cout << "ARG = " << wtServerArgs[i] << std::endl;
			wtArgv[i] = wtServerArgs[i].c_str();
		}

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (wtServerArgs.size(), const_cast<char**>(&wtArgv[0]));

		// lib init
		Av::Transcoder::init();

		// Initializing a connection pool to the database that will be shared along services
		Database::Db database {ServiceProvider<IConfig>::get()->getPath("working-dir") / "lms.db"};
		{
			Database::Session session {database};
			session.prepareTables();
			session.optimize();
		}

		UserInterface::LmsApplicationGroupContainer appGroups;

		// Service initialization order is important
		ServiceProvider<Auth::IAuthTokenService>::assign(Auth::createAuthTokenService(ServiceProvider<IConfig>::get()->getULong("login-throttler-max-entriees", 10000)));
		ServiceProvider<Auth::IPasswordService>::assign(Auth::createPasswordService(ServiceProvider<IConfig>::get()->getULong("login-throttler-max-entriees", 10000)));
		Scanner::IMediaScanner& mediaScanner {ServiceProvider<Scanner::IMediaScanner>::assign(Scanner::createMediaScanner(database))};

		Recommendation::IEngine& recommendationEngine {ServiceProvider<Recommendation::IEngine>::assign(Recommendation::createEngine(database))};
		mediaScanner.scanComplete().connect([&]()
		{
			auto status = mediaScanner.getStatus();

			if (status.lastCompleteScanStats->nbChanges() > 0 || status.lastCompleteScanStats->featuresFetched > 0)
			{
				LMS_LOG(MAIN, INFO) << "Scanner changed some files, reloading the recommendation engine...";
				recommendationEngine.requestReload();
			}
			else
			{
				LMS_LOG(MAIN, INFO) << "Scanner did not change files, not reloading the recommendation engine...";
			}
		});

		CoverArt::IGrabber& coverArtGrabber {ServiceProvider<CoverArt::IGrabber>::assign(CoverArt::createGrabber(argv[0]))};
		coverArtGrabber.setDefaultCover(server.appRoot() + "/images/unknown-cover.jpg");

		API::Subsonic::SubsonicResource subsonicResource {database};

		// bind API resources
		if (ServiceProvider<IConfig>::get()->getBool("api-subsonic", true))
			server.addResource(&subsonicResource, subsonicResource.getPath());

		// bind UI entry point
		server.addEntryPoint(Wt::EntryPointType::Application,
				std::bind(UserInterface::LmsApplication::create,
					std::placeholders::_1, std::ref(database), std::ref(appGroups)));

		// Start
		LMS_LOG(MAIN, INFO) << "Starting recommendation engine";
		recommendationEngine.start();

		LMS_LOG(MAIN, INFO) << "Starting media scanner...";
		mediaScanner.start();

		LMS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		// Wait
		LMS_LOG(MAIN, INFO) << "Now running...";
		Wt::WServer::waitForShutdown();

		// Stop
		LMS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		LMS_LOG(MAIN, INFO) << "Stopping media scanner...";
		mediaScanner.stop();

		LMS_LOG(MAIN, INFO) << "Stopping recommendation engine...";
		recommendationEngine.stop();

		ServiceProvider<CoverArt::IGrabber>::clear();

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

