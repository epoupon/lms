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

#include "Logger.hpp"

#include <Wt/WDateTime.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <thread>

#include "core/Exception.hpp"
#include "core/String.hpp"

namespace lms::core::logging
{
    const char* getModuleName(Module mod)
    {
        switch (mod)
        {
        case Module::API_SUBSONIC:
            return "API_SUBSONIC";
        case Module::AUTH:
            return "AUTH";
        case Module::AV:
            return "AV";
        case Module::CHILDPROCESS:
            return "CHILDPROC";
        case Module::COVER:
            return "COVER";
        case Module::DB:
            return "DB";
        case Module::DBUPDATER:
            return "DB UPDATER";
        case Module::FEATURE:
            return "FEATURE";
        case Module::FEEDBACK:
            return "FEEDBACK";
        case Module::HTTP:
            return "HTTP";
        case Module::MAIN:
            return "MAIN";
        case Module::METADATA:
            return "METADATA";
        case Module::REMOTE:
            return "REMOTE";
        case Module::SCROBBLING:
            return "SCROBBLING";
        case Module::SERVICE:
            return "SERVICE";
        case Module::RECOMMENDATION:
            return "RECOMMENDATION";
        case Module::TRANSCODING:
            return "TRANSCODING";
        case Module::UI:
            return "UI";
        case Module::UTILS:
            return "UTILS";
        case Module::WT:
            return "WT";
        }
        return "";
    }

    const char* getSeverityName(Severity sev)
    {
        switch (sev)
        {
        case Severity::FATAL:
            return "fatal";
        case Severity::ERROR:
            return "error";
        case Severity::WARNING:
            return "warning";
        case Severity::INFO:
            return "info";
        case Severity::DEBUG:
            return "debug";
        }
        return "";
    }

    Log::Log(ILogger& logger, Module module, Severity severity)
        : _logger{ logger }
        , _module{ module }
        , _severity{ severity }

    {
    }

    Log::~Log()
    {
        assert(_logger.isSeverityActive(_severity));
        _logger.processLog(*this);
    }

    std::string Log::getMessage() const
    {
        return _oss.str();
    }

    std::unique_ptr<ILogger> createLogger(Severity minSeverity, const std::filesystem::path& logFilePath)
    {
        return std::make_unique<Logger>(minSeverity, logFilePath);
    }

    Logger::OutputStream::OutputStream(std::ostream& os)
        : stream{ os }
    {
    }

    Logger::Logger(Severity minSeverity, const std::filesystem::path& logFilePath)
    {
        if (!logFilePath.empty())
        {
            _logFileStream = std::make_unique<std::ofstream>(logFilePath, std::ios::out | std::ios::app);
            if (!_logFileStream->is_open())
            {
                const std::error_code ec{ errno, std::generic_category() };
                throw LmsException{ "Cannot open log file '" + logFilePath.string() + "' for writing: " + ec.message() };
            }
        }

        switch (minSeverity)
        {
        case core::logging::Severity::DEBUG:
            if (_logFileStream)
                addOutputStream(*_logFileStream, { core::logging::Severity::DEBUG });
            else
                addOutputStream(std::cout, { core::logging::Severity::DEBUG });
            [[fallthrough]];
        case core::logging::Severity::INFO:
            if (_logFileStream)
                addOutputStream(*_logFileStream, { core::logging::Severity::INFO });
            else
                addOutputStream(std::cout, { core::logging::Severity::INFO });
            [[fallthrough]];
        case core::logging::Severity::WARNING:
            if (_logFileStream)
                addOutputStream(*_logFileStream, { core::logging::Severity::WARNING });
            else
                addOutputStream(std::cerr, { core::logging::Severity::WARNING });
            [[fallthrough]];
        case core::logging::Severity::ERROR:
            if (_logFileStream)
                addOutputStream(*_logFileStream, { core::logging::Severity::ERROR });
            else
                addOutputStream(std::cerr, { core::logging::Severity::ERROR });
            [[fallthrough]];
        case core::logging::Severity::FATAL:
            if (_logFileStream)
                addOutputStream(*_logFileStream, { core::logging::Severity::FATAL });
            else
                addOutputStream(std::cerr, { core::logging::Severity::FATAL });
            break;
        }
    }

    Logger::~Logger() = default;

    void Logger::addOutputStream(std::ostream& os, Severity severity)
    {
        auto it{ std::find_if(_outputStreams.begin(), _outputStreams.end(), [&os](const OutputStream& outputStream) { return &outputStream.stream == &os; }) };
        if (it == _outputStreams.end())
            it = _outputStreams.emplace(_outputStreams.end(), os);

        assert(!_severityToOutputStreamMap.contains(severity));
        _severityToOutputStreamMap.emplace(severity, &(*it));
    }

    bool Logger::isSeverityActive(Severity severity) const
    {
        return _severityToOutputStreamMap.contains(severity);
    }

    void Logger::processLog(const Log& log)
    {
        processLog(log.getModule(), log.getSeverity(), log.getMessage());
    }

    void Logger::processLog(Module module, Severity severity, std::string_view message)
    {
        assert(isSeverityActive(severity)); // should have been filtered out by a isSeverityActive call
        OutputStream* outputStream{ _severityToOutputStreamMap.at(severity) };
        const Wt::WDateTime now{ Wt::WDateTime::currentDateTime() };

        std::unique_lock lock{ outputStream->mutex };
        outputStream->stream << stringUtils::toISO8601String(now) << " " << std::this_thread::get_id() << " [" << getSeverityName(severity) << "] [" << getModuleName(module) << "] " << message << std::endl;
    }

} // namespace lms::core::logging