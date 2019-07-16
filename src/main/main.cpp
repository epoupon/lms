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

#include <boost/filesystem.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <Wt/WServer.h>
#include <Wt/WApplication.h>

#include "api/subsonic/SubsonicResource.hpp"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "cover/CoverArtGrabber.hpp"
#include "image/Image.hpp"
#include "scanner/MediaScanner.hpp"
#include "similarity/features/SimilarityFeaturesScannerAddon.hpp"
#include "similarity/SimilaritySearcher.hpp"
#include "ui/LmsApplication.hpp"
#include "utils/Config.hpp"
#include "utils/Logger.hpp"
#include "Service.hpp"

std::vector<std::string> generateWtConfig(std::string execPath)
{
	std::vector<std::string> args;

	const boost::filesystem::path wtConfigPath {Config::instance().getPath("working-dir") / "wt_config.xml"};
	const boost::filesystem::path wtLogFilePath {Config::instance().getPath("working-dir") / "lms.log"};
	const boost::filesystem::path wtAccessLogFilePath {Config::instance().getPath("working-dir") / "lms.access.log"};

	args.push_back(execPath);
	args.push_back("--config=" + wtConfigPath.string());
	args.push_back("--docroot=" + Config::instance().getString("docroot"));
	args.push_back("--approot=" + Config::instance().getString("approot"));
	args.push_back("--resources-dir=" + Config::instance().getString("wt-resources"));

	if (Config::instance().getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( Config::instance().getULong("listen-port", 5082)));
		args.push_back("--https-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + Config::instance().getString("tls-cert"));
		args.push_back("--ssl-private-key=" + Config::instance().getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + Config::instance().getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( Config::instance().getULong("listen-port", 5082)));
		args.push_back("--http-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
	}

	args.push_back("--accesslog=" + wtAccessLogFilePath.string());

	// Generate the wt_config.xml file
	boost::property_tree::ptree pt;

	pt.put("server.application-settings.<xmlattr>.location", "*");
	pt.put("server.application-settings.log-file", wtLogFilePath.string());
	pt.put("server.application-settings.log-config", Config::instance().getString("log-config", "* -debug -info:WebRequest"));
	pt.put("server.application-settings.behind-reverse-proxy", Config::instance().getBool("behind-reverse-proxy", false));
	pt.put("server.application-settings.progressive-bootstrap", true);

	std::ofstream oss(wtConfigPath.string().c_str(), std::ios::out);
	boost::property_tree::xml_parser::write_xml(oss, pt);

	return args;
}


int main(int argc, char* argv[])
{
	boost::filesystem::path configFilePath = "/etc/lms.conf";
	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	if (argc >= 2)
		configFilePath = std::string(argv[1], 0, 256);

	try
	{
		// Make pstream work with ffmpeg
		close(STDIN_FILENO);

		Config::instance().setFile(configFilePath);

		// Make sure the working directory exists
		boost::filesystem::create_directories(Config::instance().getPath("working-dir"));
		boost::filesystem::create_directories(Config::instance().getPath("working-dir") / "cache");

		// Construct WT configuration and get the argc/argv back
		std::vector<std::string> wtServerArgs = generateWtConfig(argv[0]);

		const char* wtArgv[wtServerArgs.size()];
		for (std::size_t i = 0; i < wtServerArgs.size(); ++i)
		{
			std::cout << "ARG = " << wtServerArgs[i] << std::endl;
			wtArgv[i] = wtServerArgs[i].c_str();
		}

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (wtServerArgs.size(), const_cast<char**>(wtArgv));

		// lib init
		Image::init(argv[0]);
		Av::AvInit();
		Av::Transcoder::init();
		Database::Session::configureAuth();

		// Initializing a connection pool to the database that will be shared along services
		Database::Database database {Config::instance().getPath("working-dir") / "lms.db"};

		UserInterface::LmsApplicationGroupContainer appGroups;

		// Service initialization order is important
		Scanner::MediaScanner& mediaScanner {ServiceProvider<Scanner::MediaScanner>::create(database.createSession())};

		Similarity::FeaturesScannerAddon similarityFeaturesScannerAddon {database.createSession()};

		mediaScanner.setAddon(similarityFeaturesScannerAddon);

		CoverArt::Grabber& coverArtGrabber {ServiceProvider<CoverArt::Grabber>::create()};
		coverArtGrabber.setDefaultCover(server.appRoot() + "/images/unknown-cover.jpg");

		ServiceProvider<Similarity::Searcher>::create(similarityFeaturesScannerAddon);

		API::Subsonic::SubsonicResource subsonicResource {database};

		// bind API resources
		if (Config::instance().getBool("api-subsonic", true))
		{
			for (const std::string& path : API::Subsonic::SubsonicResource::getPaths())
				server.addResource(&subsonicResource, path);
		}

		// bind UI entry point
		server.addEntryPoint(Wt::EntryPointType::Application,
				std::bind(UserInterface::LmsApplication::create,
					std::placeholders::_1, std::ref(database), std::ref(appGroups)));

		// Start
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

		LMS_LOG(MAIN, INFO) << "Clean stop!";
		res = EXIT_SUCCESS;
	}
	catch( libconfig::FileIOException& e)
	{
		std::cerr << "Cannot open config file '" << configFilePath << "'" << std::endl;
	}
	catch( libconfig::ParseException& e)
	{
		std::cerr << "Caught libconfig::ParseException! error='" << e.getError() << "', file = '" << e.getFile() << "', line = " << e.getLine() << std::endl;
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

