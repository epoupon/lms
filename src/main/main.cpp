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

#include "api/subsonic/SubsonicResource.hpp"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "auth/AuthTokenService.hpp"
#include "auth/PasswordService.hpp"
#include "cover/CoverArtGrabber.hpp"
#include "database/Db.hpp"
#include "image/Image.hpp"
#include "localplayer/LocalPlayer.hpp"
#include "localplayer/pulseaudio/PulseAudioOutput.hpp"
#include "scanner/MediaScanner.hpp"
#include "similarity/features/SimilarityFeaturesScannerAddon.hpp"
#include "similarity/SimilaritySearcher.hpp"
#include "ui/LmsApplication.hpp"
#include "utils/ChildProcessManager.hpp"
#include "utils/Config.hpp"
#include "utils/Service.hpp"
#include "utils/WtLogger.hpp"

std::vector<std::string> generateWtConfig(std::string execPath)
{
	std::vector<std::string> args;

	const std::filesystem::path wtConfigPath {ServiceProvider<Config>::get()->getPath("working-dir") / "wt_config.xml"};
	const std::filesystem::path wtLogFilePath {ServiceProvider<Config>::get()->getPath("log-file", "/var/log/lms.log")};
	const std::filesystem::path wtAccessLogFilePath {ServiceProvider<Config>::get()->getPath("access-log-file", "/var/log/lms.access.log")};

	args.push_back(execPath);
	args.push_back("--config=" + wtConfigPath.string());
	args.push_back("--docroot=" + ServiceProvider<Config>::get()->getString("docroot"));
	args.push_back("--approot=" + ServiceProvider<Config>::get()->getString("approot"));
	args.push_back("--resources-dir=" + ServiceProvider<Config>::get()->getString("wt-resources"));

	if (ServiceProvider<Config>::get()->getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( ServiceProvider<Config>::get()->getULong("listen-port", 5082)));
		args.push_back("--https-address=" + ServiceProvider<Config>::get()->getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + ServiceProvider<Config>::get()->getString("tls-cert"));
		args.push_back("--ssl-private-key=" + ServiceProvider<Config>::get()->getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + ServiceProvider<Config>::get()->getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( ServiceProvider<Config>::get()->getULong("listen-port", 5082)));
		args.push_back("--http-address=" + ServiceProvider<Config>::get()->getString("listen-addr", "0.0.0.0"));
	}

	if (!wtAccessLogFilePath.empty())
		args.push_back("--accesslog=" + wtAccessLogFilePath.string());

	// Generate the wt_config.xml file
	boost::property_tree::ptree pt;

	pt.put("server.application-settings.<xmlattr>.location", "*");
	pt.put("server.application-settings.log-file", wtLogFilePath.string());
	pt.put("server.application-settings.log-config", ServiceProvider<Config>::get()->getString("log-config", "* -debug -info:WebRequest"));
	pt.put("server.application-settings.behind-reverse-proxy", ServiceProvider<Config>::get()->getBool("behind-reverse-proxy", false));
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

		ServiceProvider<Config>::create(configFilePath);
		ServiceProvider<Logger>::create<WtLogger>();

		// Make sure the working directory exists
		std::filesystem::create_directories(ServiceProvider<Config>::get()->getPath("working-dir"));
		std::filesystem::create_directories(ServiceProvider<Config>::get()->getPath("working-dir") / "cache");

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
		Av::Transcoder::init();

		// Initializing a connection pool to the database that will be shared along services
		Database::Db database {ServiceProvider<Config>::get()->getPath("working-dir") / "lms.db"};
		{
			Database::Session session {database};
			session.prepareTables();
		}

		UserInterface::LmsApplicationGroupContainer appGroups;

		// Service initialization order is important
		ServiceProvider<Auth::AuthTokenService>::create(ServiceProvider<Config>::get()->getULong("login-throttler-max-entriees", 10000));
		ServiceProvider<Auth::PasswordService>::create(ServiceProvider<Config>::get()->getULong("login-throttler-max-entriees", 10000));
		Scanner::MediaScanner& mediaScanner {ServiceProvider<Scanner::MediaScanner>::create(database)};

		Similarity::FeaturesScannerAddon similarityFeaturesScannerAddon {database};

		mediaScanner.setAddon(similarityFeaturesScannerAddon);

		CoverArt::Grabber& coverArtGrabber {ServiceProvider<CoverArt::Grabber>::create()};
		coverArtGrabber.setDefaultCover(server.appRoot() + "/images/unknown-cover.jpg");

		ServiceProvider<Similarity::Searcher>::create(similarityFeaturesScannerAddon);

		IChildProcessManager& childProcessManager {ServiceProvider<IChildProcessManager>::create<ChildProcessManager>()};

		// Local player
		LocalPlayer& localPlayer {ServiceProvider<LocalPlayer>::create(database)};
		localPlayer.setAudioOutput(std::make_unique<PulseAudioOutput>(AudioOutput::Format::S16LE, 44100, 2));

		API::Subsonic::SubsonicResource subsonicResource {database};

		// bind API resources
		if (ServiceProvider<Config>::get()->getBool("api-subsonic", true))
			server.addResource(&subsonicResource, subsonicResource.getPath());

		// bind UI entry point
		server.addEntryPoint(Wt::EntryPointType::Application,
				std::bind(UserInterface::LmsApplication::create,
					std::placeholders::_1, std::ref(database), std::ref(appGroups)));

		// Start
		LMS_LOG(MAIN, INFO) << "Starting child process manager...";
		childProcessManager.start();

		LMS_LOG(MAIN, INFO) << "Starting media scanner...";
		mediaScanner.start();

		LMS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		LMS_LOG(MAIN, INFO) << "Starting local player...";
		localPlayer.start();


		// Wait
		LMS_LOG(MAIN, INFO) << "Now running...";
		Wt::WServer::waitForShutdown();

		// Stop
		LMS_LOG(MAIN, INFO) << "Stopping local player...";
		localPlayer.stop();

		LMS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		LMS_LOG(MAIN, INFO) << "Stopping media scanner...";
		mediaScanner.stop();

		LMS_LOG(MAIN, INFO) << "Stopping child process manager...";
		childProcessManager.stop();

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

