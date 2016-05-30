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

#include <Wt/WServer>

#include "config/Config.hpp"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "logger/Logger.hpp"
#include "image/Image.hpp"
#include "feature/FeatureExtractor.hpp"

#include "database/DatabaseUpdater.hpp"
#include "database/DatabaseFeatureExtractor.hpp"
#include "database/cluster/DatabaseHighLevelCluster.hpp"

#include "ui/LmsApplication.hpp"


static std::vector<std::string> getWtArgs(std::string path)
{
	std::vector<std::string> args;

	args.push_back(path);
	args.push_back("-c" + Config::instance().getString("wt-config"));
	args.push_back("--docroot=" + Config::instance().getString("docroot"));
	args.push_back("--approot=" + Config::instance().getString("approot"));

	if (Config::instance().getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( Config::instance().getULong("listen-port", 5081)));
		args.push_back("--https-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + Config::instance().getString("tls-cert"));
		args.push_back("--ssl-private-key=" + Config::instance().getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + Config::instance().getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( Config::instance().getULong("listen-port", 5081)));
		args.push_back("--http-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
	}

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

		std::vector<std::string> wtArgs = getWtArgs(argv[0]);

		// Construct argc/argv for Wt
		const char* wtArgv[wtArgs.size()];
		for (std::size_t i = 0; i < wtArgs.size(); ++i)
			wtArgv[i] = wtArgs[i].c_str();

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (wtArgs.size(), const_cast<char**>(wtArgv));

		Wt::WServer::instance()->logger().configure("*"); // log everything, TODO configure this

		// lib init
		Image::init(argv[0]);
		Av::AvInit();
		Av::Transcoder::init();
		Database::Handler::configureAuth();
		Feature::Extractor::init();

		// Initializing a connection pool to the database that will be shared along services
		std::unique_ptr<Wt::Dbo::SqlConnectionPool>
			connectionPool( Database::Handler::createConnectionPool(Config::instance().getString("db-path")));

		Database::Updater& dbUpdater = Database::Updater::instance();
		dbUpdater.setConnectionPool(*connectionPool);

		Database::FeatureExtractor dbFeatureExtractor;
		Database::HighLevelCluster dbHighLevelCluster;

		// Connect to the update events
		dbUpdater.scanComplete().connect(std::bind(&Database::HighLevelCluster::processDatabaseUpdate, &dbHighLevelCluster, std::placeholders::_1));
		dbUpdater.scanComplete().connect(std::bind(&Database::FeatureExtractor::processDatabaseUpdate, &dbFeatureExtractor, std::placeholders::_1));

//		dbHighLevelCluster.processDatabaseUpdate(Database::Updater::Stats());

		// bind entry point
		server.addEntryPoint(Wt::Application, boost::bind(UserInterface::LmsApplication::create,
					_1, boost::ref(*connectionPool)));

		// Start
		LMS_LOG(MAIN, INFO) << "Starting database updater...";
		dbUpdater.start();

		LMS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		// Wait
		LMS_LOG(MAIN, INFO) << "Now running...";
		Wt::WServer::waitForShutdown(argv[0]);

		// Stop
		LMS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		LMS_LOG(MAIN, INFO) << "Stopping database updater...";
		dbUpdater.stop();

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

