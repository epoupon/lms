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

#include <thread>

#include "utils/StreamLogger.hpp"

StreamLogger::StreamLogger(std::ostream& os, EnumSet<Severity> severities)
    : _os{ os }
    , _severities{ severities }
{
}

void StreamLogger::processLog(const Log& log)
{
    if (_severities.contains(log.getSeverity()))
        _os << std::this_thread::get_id() << " [" << getSeverityName(log.getSeverity()) << "] [" << getModuleName(log.getModule()) << "] " << log.getMessage() << std::endl;
}

