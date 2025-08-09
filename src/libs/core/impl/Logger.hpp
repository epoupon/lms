/*
 * Copyright (C) 2019 Emeric Poupon
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
#include <iosfwd>
#include <list>
#include <mutex>
#include <unordered_map>

#include "core/ILogger.hpp"

namespace lms::core::logging
{
    class Logger final : public ILogger
    {
    public:
        Logger(Severity minSeverity, const std::filesystem::path& logFilePath);
        ~Logger() override;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

    private:
        bool isSeverityActive(Severity severity) const override;
        void processLog(const Log& log) override;
        void processLog(Module module, Severity severity, std::string_view message) override;

        void addOutputStream(std::ostream& os, Severity severity);

        struct OutputStream
        {
            OutputStream(std::ostream& os);

            std::mutex mutex;
            std::ostream& stream;
        };

        std::list<OutputStream> _outputStreams;
        std::unordered_map<Severity, OutputStream*> _severityToOutputStreamMap;
        std::unique_ptr<std::ofstream> _logFileStream;
    };
} // namespace lms::core::logging