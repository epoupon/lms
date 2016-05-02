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

#include "config/config.h"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "logger/Logger.hpp"
#include "image/Image.hpp"

#include "database/DatabaseUpdater.hpp"
#include "database/DatabaseClassifier.hpp"
#include "feature/FeatureExtractor.hpp"
#include "ui/LmsApplication.hpp"


int main(int argc, char* argv[])
{
	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	try
	{
		// Make pstream work with ffmpeg
		close(STDIN_FILENO);

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (argc, argv);

		Wt::WServer::instance()->logger().configure("*"); // log everything

		// lib init
		Image::init(argv[0]);
		Av::AvInit();
		Av::Transcoder::init();
		Database::Handler::configureAuth();
		Feature::Extractor::init();

		// Initializing a connection pool to the database that will be shared along services
		std::unique_ptr<Wt::Dbo::SqlConnectionPool> connectionPool( Database::Handler::createConnectionPool("/var/lms/lms.db")); // TODO use $datadir from autotools

		Database::Updater& dbUpdater = Database::Updater::instance();
		dbUpdater.setConnectionPool(*connectionPool);

		Database::Classifier dbClassifier(*connectionPool);

		// Connect the classifier to the update events
		dbUpdater.trackChanged().connect(std::bind(&Database::Classifier::processTrackUpdate, &dbClassifier, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		dbUpdater.scanComplete().connect(std::bind(&Database::Classifier::processDatabaseUpdate, &dbClassifier, std::placeholders::_1));

		// bind entry point
		server.addEntryPoint(Wt::Application, boost::bind(UserInterface::LmsApplication::create, _1, boost::ref(*connectionPool)));

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
	catch( Wt::WServer::Exception& e)
	{
		std::cerr << "Caught a WServer::Exception: " << e.what();
	}
	catch( std::exception& e)
	{
		std::cerr << "Caught std::exception: " << e.what();
	}

	return res;
}

