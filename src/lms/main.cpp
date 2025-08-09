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

#include <Wt/WApplication.h>
#include <Wt/WLogSink.h>
#include <Wt/WServer.h>
#include <boost/asio/io_context.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "core/IChildProcessManager.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/IOContextRunner.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"
#include "core/SystemPaths.hpp"
#include "database/IDb.hpp"
#include "database/IQueryPlanRecorder.hpp"
#include "database/Session.hpp"
#include "image/Image.hpp"
#include "services/artwork/IArtworkService.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IEnvService.hpp"
#include "services/auth/IPasswordService.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IPlaylistGeneratorService.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scanner/IScannerService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "services/transcoding/ITranscodingService.hpp"
#include "subsonic/SubsonicResource.hpp"
#include "ui/Auth.hpp"
#include "ui/LmsApplication.hpp"
#include "ui/LmsApplicationManager.hpp"
#include "ui/LmsInitApplication.hpp"

namespace lms
{
    namespace
    {
        std::size_t getThreadCount()
        {
            const unsigned long configHttpServerThreadCount{ core::Service<core::IConfig>::get()->getULong("http-server-thread-count", 0) };

            // Reserve at least 2 threads since we still have some blocking IO (for example when reading from ffmpeg)
            return configHttpServerThreadCount ? configHttpServerThreadCount : std::max<unsigned long>(2, std::thread::hardware_concurrency());
        }

        ui::AuthenticationBackend getUIAuthenticationBackend()
        {
            const std::string backend{ core::stringUtils::stringToLower(core::Service<core::IConfig>::get()->getString("authentication-backend", "internal")) };
            if (backend == "internal")
                return ui::AuthenticationBackend::Internal;
            if (backend == "pam")
                return ui::AuthenticationBackend::PAM;
            if (backend == "http-headers")
                return ui::AuthenticationBackend::Env;

            throw core::LmsException{ "Invalid config value for 'authentication-backend'" };
        }

        std::optional<core::tracing::Level> getTracingLevel()
        {
            std::string_view tracingLevel{ core::Service<core::IConfig>::get()->getString("tracing-level", "disabled") };

            if (tracingLevel == "disabled")
                return std::nullopt;
            else if (tracingLevel == "overview")
                return core::tracing::Level::Overview;
            else if (tracingLevel == "detailed")
                return core::tracing::Level::Detailed;

            throw core::LmsException{ "Invalid config value for 'tracing-level'" };
        }

        std::vector<std::string> generateWtConfig(std::string execPath)
        {
            core::IConfig& config{ *core::Service<core::IConfig>::get() };

            std::vector<std::string> args;

            const std::filesystem::path wtConfigPath{ config.getPath("working-dir", "/var/lms") / "wt_config.xml" };
            const std::filesystem::path wtResourcesPath{ config.getPath("wt-resources", "/usr/share/Wt/resources") };

            args.push_back(execPath);
            args.push_back("--config=" + wtConfigPath.string());
            args.push_back("--docroot=" + std::string{ config.getString("docroot", "/usr/share/lms/docroot/;/resources,/css,/images,/js,/favicon.ico") });
            args.push_back("--approot=" + std::string{ config.getString("approot", "/usr/share/lms/approot") });
            args.push_back("--deploy-path=" + std::string{ config.getString("deploy-path", "/") });
            if (!wtResourcesPath.empty())
                args.push_back("--resources-dir=" + wtResourcesPath.string());

            if (core::Service<core::IConfig>::get()->getBool("tls-enable", false))
            {
                args.push_back("--https-port=" + std::to_string(config.getULong("listen-port", 5082)));
                args.push_back("--https-address=" + std::string{ config.getString("listen-addr", "0.0.0.0") });
                args.push_back("--ssl-certificate=" + std::string{ config.getString("tls-cert", "/var/lms/cert.pem") });
                args.push_back("--ssl-private-key=" + std::string{ config.getString("tls-key", "/var/lms/privkey.pem") });
                args.push_back("--ssl-tmp-dh=" + std::string{ config.getString("tls-dh", "/var/lms/dh2048.pem") });
            }
            else
            {
                args.push_back("--http-port=" + std::to_string(config.getULong("listen-port", 5082)));
                args.push_back("--http-address=" + std::string{ config.getString("listen-addr", "0.0.0.0") });
            }

            args.push_back("--threads=" + std::to_string(getThreadCount()));

            // Generate the wt_config.xml file
            boost::property_tree::ptree pt;

            pt.put("server.application-settings.<xmlattr>.location", "*");

            // Reverse proxy
            if (config.getBool("behind-reverse-proxy", false))
            {
                pt.put("server.application-settings.trusted-proxy-config.original-ip-header", config.getString("original-ip-header", "X-Forwarded-For"));
                config.visitStrings("trusted-proxies", [&](std::string_view trustedProxy) {
                    pt.add("server.application-settings.trusted-proxy-config.trusted-proxies.proxy", std::string{ trustedProxy });
                },
                    { "127.0.0.1", "::1" });
            }

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
                std::ofstream oss{ wtConfigPath, std::ios::out };
                if (!oss)
                    throw core::LmsException{ "Can't open '" + wtConfigPath.string() + "' for writing!" };

                boost::property_tree::xml_parser::write_xml(oss, pt);

                if (!oss)
                    throw core::LmsException{ "Can't write in file '" + wtConfigPath.string() + "', no space left?" };
            }

            return args;
        }

        void proxyScannerEventsToApplication(scanner::IScannerService& scanner, Wt::WServer& server)
        {
            auto postAll{ [](Wt::WServer& server, std::function<void()> cb) {
                server.postAll([cb = std::move(cb)] {
                    // may be nullptr, see https://redmine.webtoolkit.eu/issues/8202
                    if (LmsApp)
                        cb();
                });
            } };

            scanner.getEvents().scanAborted.connect([&] {
                postAll(server, [] {
                    LmsApp->getScannerEvents().scanAborted.emit();
                    LmsApp->triggerUpdate();
                });
            });

            scanner.getEvents().scanStarted.connect([&] {
                postAll(server, [] {
                    LmsApp->getScannerEvents().scanStarted.emit();
                    LmsApp->triggerUpdate();
                });
            });

            scanner.getEvents().scanComplete.connect([&](const scanner::ScanStats& stats) {
                postAll(server, [=] {
                    LmsApp->getScannerEvents().scanComplete.emit(stats);
                    LmsApp->triggerUpdate();
                });
            });

            scanner.getEvents().scanInProgress.connect([&](const scanner::ScanStepStats& stats) {
                postAll(server, [=] {
                    LmsApp->getScannerEvents().scanInProgress.emit(stats);
                    LmsApp->triggerUpdate();
                });
            });

            scanner.getEvents().scanScheduled.connect([&](const Wt::WDateTime dateTime) {
                postAll(server, [=] {
                    LmsApp->getScannerEvents().scanScheduled.emit(dateTime);
                    LmsApp->triggerUpdate();
                });
            });
        }

        core::logging::Severity getLogMinSeverity()
        {
            std::string_view minSeverity{ core::Service<core::IConfig>::get()->getString("log-min-severity", "info") };

            if (minSeverity == "debug")
                return core::logging::Severity::DEBUG;
            else if (minSeverity == "info")
                return core::logging::Severity::INFO;
            else if (minSeverity == "warning")
                return core::logging::Severity::WARNING;
            else if (minSeverity == "error")
                return core::logging::Severity::ERROR;
            else if (minSeverity == "fatal")
                return core::logging::Severity::FATAL;

            throw core::LmsException{ "Invalid config value for 'log-min-severity'" };
        }

        class LmsLogSink : public Wt::WLogSink
        {
        public:
            LmsLogSink(core::logging::ILogger& logger)
                : _logger{ logger }
            {
            }

        private:
            void log(const std::string& type, const std::string& scope, const std::string& message) const noexcept override
            {
                // Some wt code path may go here without testing logging()
                if (logging(type, scope))
                {
                    const core::logging::Severity severity{ getSeverity(type, scope) };
                    _logger.processLog(core::logging::Module::WT, severity, message);
                }
            }

            bool logging(const std::string& type, const std::string& scope) const noexcept override
            {
                const core::logging::Severity severity{ getSeverity(type, scope) };
                return _logger.isSeverityActive(severity);
            }

            static core::logging::Severity getSeverity(const std::string& type, const std::string& scope)
            {
                return adjustSeverity(getSeverityFromString(type), scope);
            }

            static core::logging::Severity adjustSeverity(core::logging::Severity initialSeverity, std::string_view scope)
            {
                if (initialSeverity == core::logging::Severity::INFO && (scope == "WebRequest" || scope == "wthttp"))
                    return core::logging::Severity::DEBUG;

                return initialSeverity;
            }

            static core::logging::Severity getSeverityFromString(std::string_view type)
            {
                if (type == "debug")
                    return core::logging::Severity::DEBUG;
                if (type == "info")
                    return core::logging::Severity::INFO;
                if (type == "warning")
                    return core::logging::Severity::WARNING;
                if (type == "error")
                    return core::logging::Severity::ERROR;
                if (type == "fatal")
                    return core::logging::Severity::FATAL;

                return core::logging::Severity::INFO;
            }

            core::logging::ILogger& _logger;
        };

    } // namespace

    int main(int argc, char* argv[])
    {
        std::filesystem::path configFilePath{ core::sysconfDirectory / "lms.conf" };
        int res{ EXIT_FAILURE };

        assert(argc > 0);
        assert(argv[0] != NULL);

        auto displayUsage{ [&](std::ostream& os) {
            os << "Usage:\t" << argv[0] << "\t[conf_file]\n\n"
               << "Options:\n"
               << "\tconf_file:\t path to the LMS configuration file (defaults to " << configFilePath << ")\n\n";
        } };

        if (argc == 2)
        {
            const std::string_view arg{ argv[1] };
            if (arg == "-h" || arg == "--help")
            {
                displayUsage(std::cout);
                return EXIT_SUCCESS;
            }
            configFilePath = std::string(arg, 0, 256);
        }
        else if (argc > 2)
        {
            displayUsage(std::cerr);
            return EXIT_FAILURE;
        }

        try
        {
            close(STDIN_FILENO);

            core::Service<core::IConfig> config{ core::createConfig(configFilePath) };
            core::Service<core::logging::ILogger> logger{ createLogger(getLogMinSeverity(), config->getPath("log-file", "")) };
            core::Service<core::tracing::ITraceLogger> traceLogger;
            if (const auto level{ getTracingLevel() })
                traceLogger.assign(core::tracing::createTraceLogger(level.value(), config->getULong("tracing-buffer-size", core::tracing::MinBufferSizeInMBytes)));

            // use system locale. libarchive relies on this to write filenames
            if (char* locale{ ::setlocale(LC_ALL, "") })
                LMS_LOG(MAIN, INFO, "locale set to '" << locale << "'");
            else
                LMS_LOG(MAIN, WARNING, "Cannot set locale from system");

            // Make sure the working directory exists
            std::filesystem::create_directories(config->getPath("working-dir", "/var/lms"));
            std::filesystem::create_directories(config->getPath("working-dir", "/var/lms") / "cache");

            // Construct WT configuration and get the argc/argv back
            const std::vector<std::string> wtServerArgs{ generateWtConfig(argv[0]) };

            std::vector<const char*> wtArgv(wtServerArgs.size());
            for (std::size_t i = 0; i < wtServerArgs.size(); ++i)
            {
                std::cout << "ARG = " << wtServerArgs[i] << std::endl;
                wtArgv[i] = wtServerArgs[i].c_str();
            }

            LmsLogSink lmsLogSink{ *logger };
            Wt::WServer server{ argv[0] };
            server.setCustomLogger(lmsLogSink);
            server.setServerConfiguration(wtServerArgs.size(), const_cast<char**>(&wtArgv[0]));

            // As initialization can take a while (db migration, analyze, etc.), we bind a temporary init entry point to warn the user
            server.addEntryPoint(Wt::EntryPointType::Application,
                [&](const Wt::WEnvironment& env) {
                    return ui::LmsInitApplication::create(env);
                });

            LMS_LOG(MAIN, INFO, "Starting init web server...");
            server.start();

            boost::asio::io_context ioContext; // ioContext used to dispatch all the services that are out of the Wt event loop
            core::IOContextRunner ioContextRunner{ ioContext, getThreadCount(), "Misc" };

            core::Service<db::IQueryPlanRecorder> queryPlanRecorder;
            if (config->getBool("db-record-query-plans", false))
                queryPlanRecorder.assign(db::createQueryPlanRecorder());

            // Connection pool size must be twice the number of threads: we have at least 2 io pools with getThreadCount() each and they all may access the database
            auto database{ db::createDb(config->getPath("working-dir", "/var/lms") / "lms.db", getThreadCount() * 2) };
            {
                db::Session session{ *database };
                session.prepareTablesIfNeeded();
                bool migrationPerformed{ session.migrateSchemaIfNeeded() };
                session.createIndexesIfNeeded();

                // As this may be quite long, we only do it during startup
                if (migrationPerformed)
                    session.vacuum();
                else
                    session.vacuumIfNeeded();
            }

            ui::LmsApplicationManager appManager;

            const std::size_t loginThrottlerMaxEntries{ config->getULong("login-throttler-max-entries", 10'000) };
            // Service initialization order is important (reverse-order for deinit)
            core::Service<core::IChildProcessManager> childProcessManagerService{ core::createChildProcessManager(ioContext) };

            const ui::AuthenticationBackend uiAuthenticationBackend{ getUIAuthenticationBackend() };
            core::Service<auth::IAuthTokenService> authTokenService{ auth::createAuthTokenService(*database, config->getULong("login-throttler-max-entriees", 10'000)) };
            core::Service<auth::IPasswordService> authPasswordService;
            core::Service<auth::IEnvService> authEnvService;

            authTokenService->registerDomain("ui", auth::IAuthTokenService::DomainParameters{
                                                       .tokenMaxUseCount = 1,
                                                       .tokenDuration = std::chrono::weeks{ 8 },
                                                   });

            authTokenService->registerDomain("subsonic", auth::IAuthTokenService::DomainParameters{
                                                             .tokenMaxUseCount = std::nullopt, // no usage limit
                                                             .tokenDuration = std::nullopt,    // no time limit
                                                         });

            switch (uiAuthenticationBackend)
            {
            case ui::AuthenticationBackend::Internal:
                authPasswordService.assign(auth::createPasswordService("internal", *database, loginThrottlerMaxEntries));
                break;
            case ui::AuthenticationBackend::PAM:
                authPasswordService.assign(auth::createPasswordService("PAM", *database, loginThrottlerMaxEntries));
                break;
            case ui::AuthenticationBackend::Env:
                authEnvService.assign(auth::createEnvService("http-headers", *database));
                break;
            }

            image::init(argv[0]);
            core::Service<artwork::IArtworkService> artworkService{ artwork::createArtworkService(*database, server.appRoot() + "/images/unknown-cover.svg", server.appRoot() + "/images/unknown-artist.svg") };
            core::Service<recommendation::IRecommendationService> recommendationService{ recommendation::createRecommendationService(*database) };
            core::Service<recommendation::IPlaylistGeneratorService> playlistGeneratorService{ recommendation::createPlaylistGeneratorService(*database, *recommendationService) };
            core::Service<scanner::IScannerService> scannerService{ scanner::createScannerService(*database) };
            core::Service<transcoding::ITranscodingService> transcodingService{ transcoding::createTranscodingService(*database, *childProcessManagerService) };

            scannerService->getEvents().scanComplete.connect([&] {
                // Flush cover cache even if no changes:
                // covers may be external files that changed and we don't keep track of them for now (but we should)
                artworkService->flushCache();
            });

            core::Service<feedback::IFeedbackService> feedbackService{ feedback::createFeedbackService(ioContext, *database) };
            core::Service<scrobbling::IScrobblingService> scrobblingService{ scrobbling::createScrobblingService(ioContext, *database) };

            LMS_LOG(MAIN, INFO, "Stopping init web server...");
            server.stop();

            server.removeEntryPoint("");

            std::unique_ptr<Wt::WResource> subsonicResource;
            // bind API resources
            if (config->getBool("api-subsonic", true))
            {
                subsonicResource = api::subsonic::createSubsonicResource(*database);
                server.addResource(subsonicResource.get(), "/rest");
            }

            // bind UI entry point
            server.addEntryPoint(Wt::EntryPointType::Application,
                [&database, &appManager, uiAuthenticationBackend](const Wt::WEnvironment& env) {
                    return ui::LmsApplication::create(env, *database, appManager, uiAuthenticationBackend);
                });

            proxyScannerEventsToApplication(*scannerService, server);

            LMS_LOG(MAIN, INFO, "Starting init web server...");
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
} // namespace lms

int main(int argc, char* argv[])
{
    return lms::main(argc, argv);
}
