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

#include "core/WtLogger.hpp"

#include <thread>
#include <sstream>
#include <Wt/WServer.h>
#include <Wt/WLogger.h>

#include "core/Exception.hpp"

namespace lms::core::logging
{
    namespace
    {
        std::string to_string(std::thread::id id)
        {
            std::ostringstream oss;
            oss << id;
            return oss.str();
        }
    }

    WtLogger::WtLogger(Severity minSeverity)
        : _minSeverity{ minSeverity }
    {
    }

    std::string WtLogger::computeLogConfig(Severity minSeverity)
    {
        switch (minSeverity)
        {
        case Severity::DEBUG:       return "*";
        case Severity::INFO:        return "* -debug";
        case Severity::WARNING:     return "* -debug -info";
        case Severity::ERROR:       return "* -debug -info -warning";
        case Severity::FATAL:       return "* -debug -info -warning -error";
        }

        throw LmsException{ "Unhandled severity" };
    }

    bool WtLogger::isSeverityActive(Severity severity) const
    {
        return static_cast<int>(severity) <= static_cast<int>(_minSeverity);
    }

    void WtLogger::processLog(const Log& log)
    {
        Wt::log(getSeverityName(log.getSeverity())) << Wt::WLogger::sep << to_string(std::this_thread::get_id()) << Wt::WLogger::sep << "[" << getModuleName(log.getModule()) << "]" << Wt::WLogger::sep << log.getMessage();
    }
}