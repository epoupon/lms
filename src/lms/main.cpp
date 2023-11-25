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

#include <boost/asio/io_context.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <Wt/WServer.h>
#include <Wt/WApplication.h>

#include "image/IRawImage.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IPasswordService.hpp"
#include "services/auth/IEnvService.hpp"
#include "services/cover/ICoverService.hpp"
#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IPlaylistGeneratorService.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scanner/IScannerService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "subsonic/SubsonicResource.hpp"
#include "ui/LmsApplication.hpp"
#include "ui/LmsApplicationManager.hpp"
#include "utils/IChildProcessManager.hpp"
#include "utils/IConfig.hpp"
#include "utils/IOContextRunner.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "utils/WtLogger.hpp"

namespace
{
    std::size_t getThreadCount()
    {
        const unsigned long configHttpServerThreadCount{ Service<IConfig>::get()->getULong("http-server-thread-count", 0) };

        // Reserve at least 2 threads since we still have some blocking IO (for example when reading from ffmpeg)
        return configHttpServerThreadCount ? configHttpServerThreadCount : std::max<unsigned long>(2, std::thread::hardware_concurrency());
    }

    Severity getLogMinSeverity()
    {
        std::string_view minSeverity{ Service<IConfig>::get()->getString("log-min-severity", "info") };

        if (minSeverity == "debug")
            return Severity::DEBUG;
        else if (minSeverity == "info")
            return Severity::INFO;
        else if (minSeverity == "warning")
            return Severity::WARNING;
        else if (minSeverity == "error")
            return Severity::ERROR;
        else if (minSeverity == "fatal")
            return Severity::FATAL;

        throw LmsException{ "Invalid config value for 'log-min-severity'" };
    }

    std::vector<std::string> generateWtConfig(std::string execPath, Severity minSeverity)
    {
        std::vector<std::string> args;

        const std::filesystem::path wtConfigPath{ Service<IConfig>::get()->getPath("working-dir") / "wt_config.xml" };
        const std::filesystem::path wtLogFilePath{ Service<IConfig>::get()->getPath("log-file", "/var/log/lms.log") };
        const std::filesystem::path wtAccessLogFilePath{ Service<IConfig>::get()->getPath("access-log-file", "/var/log/lms.access.log") };
        const std::filesystem::path wtResourcesPath{ Service<IConfig>::get()->getPath("wt-resources", "/usr/share/Wt/resources") };

        args.push_back(execPath);
        args.push_back("--config=" + wtConfigPath.string());
        args.push_back("--docroot=" + std::string{ Service<IConfig>::get()->getString("docroot") });
        args.push_back("--approot=" + std::string{ Service<IConfig>::get()->getString("approot") });
        args.push_back("--deploy-path=" + std::string{ Service<IConfig>::get()->getString("deploy-path", "/") });
        if (!wtResourcesPath.empty())
            args.push_back("--resources-dir=" + wtResourcesPath.string());

        if (Service<IConfig>::get()->getBool("tls-enable", false))
        {
            args.push_back("--https-port=" + std::to_string(Service<IConfig>::get()->getULong("listen-port", 5082)));
            args.push_back("--https-address=" + std::string{ Service<IConfig>::get()->getString("listen-addr", "0.0.0.0") });
            args.push_back("--ssl-certificate=" + std::string{ Service<IConfig>::get()->getString("tls-cert") });
            args.push_back("--ssl-private-key=" + std::string{ Service<IConfig>::get()->getString("tls-key") });
            args.push_back("--ssl-tmp-dh=" + std::string{ Service<IConfig>::get()->getString("tls-dh") });
        }
        else
        {
            args.push_back("--http-port=" + std::to_string(Service<IConfig>::get()->getULong("listen-port", 5082)));
            args.push_back("--http-address=" + std::string{ Service<IConfig>::get()->getString("listen-addr", "0.0.0.0") });
        }

        if (!wtAccessLogFilePath.empty())
            args.push_back("--accesslog=" + wtAccessLogFilePath.string());

        args.push_back("--threads=" + std::to_string(getThreadCount()));

        // Generate the wt_config.xml file
        boost::property_tree::ptree pt;

        pt.put("server.application-settings.<xmlattr>.location", "*");
        pt.put("server.application-settings.log-file", wtLogFilePath.string());

        // log-config
        pt.put("server.application-settings.log-config", WtLogger::computeLogConfig(minSeverity));
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
            std::ofstream oss{ wtConfigPath.string().c_str(), std::ios::out };
            if (!oss)
                throw LmsException{ "Can't open '" + wtConfigPath.string() + "' for writing!" };

            boost::property_tree::xml_parser::write_xml(oss, pt);

            if (!oss)
                throw LmsException{ "Can't write in file '" + wtConfigPath.string() + "', no space left?" };
        }

        return args;
    }

    void proxyScannerEventsToApplication(Scanner::IScannerService& scanner, Wt::WServer& server)
    {
        auto postAll{ [](Wt::WServer& server, std::function<void()> cb)
        {
            server.postAll([cb = std::move(cb)]
            {
                    // may be nullptr, see https://redmine.webtoolkit.eu/issues/8202
                    if (LmsApp)
                        cb();
                });
            } };

        scanner.getEvents().scanStarted.connect([&]
            {
                postAll(server, []
                    {
                        LmsApp->getScannerEvents().scanStarted.emit();
                        LmsApp->triggerUpdate();
                    });
            });

        scanner.getEvents().scanComplete.connect([&](const Scanner::ScanStats& stats)
            {
                postAll(server, [=]
                    {
                        LmsApp->getScannerEvents().scanComplete.emit(stats);
                        LmsApp->triggerUpdate();
                    });
            });

        scanner.getEvents().scanInProgress.connect([&](const Scanner::ScanStepStats& stats)
            {
                postAll(server, [=]
                    {
                        LmsApp->getScannerEvents().scanInProgress.emit(stats);
                        LmsApp->triggerUpdate();
                    });
            });

        scanner.getEvents().scanScheduled.connect([&](const Wt::WDateTime dateTime)
            {
                postAll(server, [=]
                    {
                        LmsApp->getScannerEvents().scanScheduled.emit(dateTime);
                        LmsApp->triggerUpdate();
                    });
            });
    }
}

int main(int argc, char* argv[])
{
    std::filesystem::path configFilePath{ "/etc/lms.conf" };
    int res{ EXIT_FAILURE };

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
        close(STDIN_FILENO);

        Service<IConfig> config{ createConfig(configFilePath) };
        const Severity minLogSeverity{getLogMinSeverity()};
        Service<ILogger> logger{ std::make_unique<WtLogger>(minLogSeverity) };

        // use system locale. libarchive relies on this to write filenames
        if (char* locale{ ::setlocale(LC_ALL, "") })
            LMS_LOG(MAIN, INFO, "locale set to '" << locale << "'");
        else
            LMS_LOG(MAIN, WARNING, "Cannot set locale from system");

        // Make sure the working directory exists
        std::filesystem::create_directories(config->getPath("working-dir"));
        std::filesystem::create_directories(config->getPath("working-dir") / "cache");

        // Construct WT configuration and get the argc/argv back
        const std::vector<std::string> wtServerArgs{ generateWtConfig(argv[0], minLogSeverity) };

        std::vector<const char*> wtArgv(wtServerArgs.size());
        for (std::size_t i = 0; i < wtServerArgs.size(); ++i)
        {
            std::cout << "ARG = " << wtServerArgs[i] << std::endl;
            wtArgv[i] = wtServerArgs[i].c_str();
        }

        boost::asio::io_context ioContext; // ioContext used to dispatch all the services that are out of the Wt event loop
        Wt::WServer server{ argv[0] };
        server.setServerConfiguration(wtServerArgs.size(), const_cast<char**>(&wtArgv[0]));

        IOContextRunner ioContextRunner{ ioContext, getThreadCount() };

        // Connection pool size must be twice the number of threads: we have at least 2 io pools with getThreadCount() each and they all may access the database
        Database::Db database{ config->getPath("working-dir") / "lms.db", getThreadCount() * 2 };
        {
            Database::Session session{ database };
            session.prepareTables();

            // force optimize in case scanner aborted during a large import:
            // queries may be too slow to even be able to relaunch a scan sing the web interface
            session.analyze();
        }

        UserInterface::LmsApplicationManager appManager;

        // Service initialization order is important (reverse-order for deinit)
        Service<IChildProcessManager> childProcessManagerService{ createChildProcessManager(ioContext) };
        Service<Auth::IAuthTokenService> authTokenService;
        Service<Auth::IPasswordService> authPasswordService;
        Service<Auth::IEnvService> authEnvService;

        const std::string authenticationBackend{ StringUtils::stringToLower(config->getString("authentication-backend", "internal")) };
        if (authenticationBackend == "internal" || authenticationBackend == "pam")
        {
            authTokenService.assign(Auth::createAuthTokenService(database, config->getULong("login-throttler-max-entriees", 10000)));
            authPasswordService.assign(Auth::createPasswordService(authenticationBackend, database, config->getULong("login-throttler-max-entriees", 10000), *authTokenService.get()));
        }
        else if (authenticationBackend == "http-headers")
        {
            authEnvService.assign(Auth::createEnvService(authenticationBackend, database));
        }
        else
            throw LmsException{ "Bad value '" + authenticationBackend + "' for 'authentication-backend'" };

        Image::init(argv[0]);
        Service<Cover::ICoverService> coverService{ Cover::createCoverService(database, argv[0], server.appRoot() + "/images/unknown-cover.jpg") };
        Service<Recommendation::IRecommendationService> recommendationService{ Recommendation::createRecommendationService(database) };
        Service<Recommendation::IPlaylistGeneratorService> playlistGeneratorService{ Recommendation::createPlaylistGeneratorService(database, *recommendationService.get()) };
        Service<Scanner::IScannerService> scannerService{ Scanner::createScannerService(database) };

        scannerService->getEvents().scanComplete.connect([&]
            {
                // Flush cover cache even if no changes:
                // covers may be external files that changed and we don't keep track of them
                coverService->flushCache();
            });

        Service<Feedback::IFeedbackService> feedbackService{ Feedback::createFeedbackService(ioContext, database) };
        Service<Scrobbling::IScrobblingService> scrobblingService{ Scrobbling::createScrobblingService(ioContext, database) };

        std::unique_ptr<Wt::WResource> subsonicResource;

        // bind API resources
        if (config->getBool("api-subsonic", true))
        {
            subsonicResource = API::Subsonic::createSubsonicResource(database);
            server.addResource(subsonicResource.get(), "/rest");
        }

        // bind UI entry point
        server.addEntryPoint(Wt::EntryPointType::Application,
            [&](const Wt::WEnvironment& env)
            {
                return UserInterface::LmsApplication::create(env, database, appManager);
            });

        proxyScannerEventsToApplication(*scannerService, server);

        LMS_LOG(MAIN, INFO, "Starting server...");
        server.start();

        LMS_LOG(MAIN, INFO, "Now running...");
        Wt::WServer::waitForShutdown();

        LMS_LOG(MAIN, INFO, "Stopping server...");
        server.stop();

        LMS_LOG(MAIN, INFO, "Quitting...");
        res = EXIT_SUCCESS;
    }
    catch (const Wt::WServer::Exception& e)
    {
        LMS_LOG(MAIN, FATAL, "Caught WServer::Exception: " << e.what());
        std::cerr << "Caught a WServer::Exception: " << e.what() << std::endl;
        res = EXIT_FAILURE;
    }
    catch (const std::exception& e)
    {
        LMS_LOG(MAIN, FATAL, "Caught std::exception: " << e.what());
        std::cerr << "Caught std::exception: " << e.what() << std::endl;
        res = EXIT_FAILURE;
    }

    return res;
}

