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

#include "config/config.h"
#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"
#include "logger/Logger.hpp"
#include "cover/CoverArtGrabber.hpp"

#include "ui/LmsApplication.hpp"

#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"
#if defined HAVE_LMSAPI
#include "service/LmsAPIServerService.hpp"
#endif

#include <Wt/WServer>

int main(int argc, char* argv[])
{
	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	try
	{
		Wt::WServer server(argv[0]);
		server.setServerConfiguration (argc, argv);

		Wt::WServer::instance()->logger().configure("*"); // log everything

		Service::ServiceManager& serviceManager = Service::ServiceManager::instance();

		// lib init
		Av::AvInit();
		Av::Transcoder::init();
		Database::Handler::configureAuth();

		// Initializing a connection pool to the database that will be shared along services
		std::unique_ptr<Wt::Dbo::SqlConnectionPool> connectionPool( Database::Handler::createConnectionPool("/var/lms/lms.db")); // TODO use $datadir from autotools

		serviceManager.add( std::make_shared<Service::DatabaseUpdateService>(*connectionPool));

#if defined HAVE_LMSAPI
		serviceManager.add( std::make_shared<Service::LmsAPIService>(*connectionPool));
#endif

		// bind entry point
		server.addEntryPoint(Wt::Application, boost::bind(UserInterface::LmsApplication::create, _1, boost::ref(*connectionPool)));

		// Starting the main server
		LMS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		// Start underlying services
		LMS_LOG(MAIN, INFO) << "Starting services...";
		serviceManager.start();

		LMS_LOG(MAIN, INFO) << "Now running...";

		// Waiting for shutdown command
		Wt::WServer::waitForShutdown(argv[0]);

		LMS_LOG(MAIN, INFO) << "Stopping services...";
		serviceManager.stop();
		serviceManager.clear();

		LMS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		res = EXIT_SUCCESS;
	}
	catch( Wt::WServer::Exception& e)
	{
		LMS_LOG(MAIN, FATAL) << "Caught a WServer::Exception: " << e.what();
	}
	catch( std::exception& e)
	{
		LMS_LOG(MAIN, FATAL) << "Caught std::exception: " << e.what();
	}

	return res;
}

