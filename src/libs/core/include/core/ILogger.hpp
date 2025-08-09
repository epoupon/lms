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

#pragma once

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

#include "core/Service.hpp"

namespace lms::core::logging
{
    enum class Severity
    {
        FATAL,
        ERROR,
        WARNING,
        INFO,
        DEBUG,
    };

    enum class Module
    {
        API_SUBSONIC,
        AUTH,
        AV,
        CHILDPROCESS,
        COVER,
        DB,
        DBUPDATER,
        FEATURE,
        FEEDBACK,
        HTTP,
        MAIN,
        METADATA,
        REMOTE,
        SCROBBLING,
        SERVICE,
        RECOMMENDATION,
        TRANSCODING,
        UI,
        UTILS,
        WT,
    };

    const char* getModuleName(Module mod);
    const char* getSeverityName(Severity sev);

    class ILogger;
    class Log
    {
    public:
        Log(ILogger& logger, Module module, Severity severity);
        ~Log();

        Module getModule() const { return _module; }
        Severity getSeverity() const { return _severity; }
        std::string getMessage() const;

        std::ostringstream& getOstream() { return _oss; }

    private:
        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;

        ILogger& _logger;
        Module _module;
        Severity _severity;
        std::ostringstream _oss;
    };

    class ILogger
    {
    public:
        virtual ~ILogger() = default;

        virtual bool isSeverityActive(Severity severity) const = 0;
        virtual void processLog(const Log& log) = 0;
        virtual void processLog(Module module, Severity severity, std::string_view message) = 0;
    };

    static constexpr Severity defaultMinSeverity{ Severity::INFO };
    std::unique_ptr<ILogger> createLogger(Severity minSeverity = defaultMinSeverity, const std::filesystem::path& logFilePath = {});
} // namespace lms::core::logging

#define LMS_LOG(module, severity, message)                                                                                                                               \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (auto* logger_{ ::lms::core::Service<::lms::core::logging::ILogger>::get() }; logger_ && logger_->isSeverityActive(::lms::core::logging::Severity::severity)) \
            ::lms::core::logging::Log{ *logger_, ::lms::core::logging::Module::module, ::lms::core::logging::Severity::severity }.getOstream() << message;               \
    } while (0)

#define LMS_LOG_IF(module, severity, cond, message)                                                                                                                              \
    do                                                                                                                                                                           \
    {                                                                                                                                                                            \
        if (auto* logger_{ ::lms::core::Service<::lms::core::logging::ILogger>::get() }; logger_ && logger_->isSeverityActive(::lms::core::logging::Severity::severity) && cond) \
            ::lms::core::logging::Log{ *logger_, ::lms::core::logging::Module::module, ::lms::core::logging::Severity::severity }.getOstream() << message;                       \
    } while (0)
