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

#include "utils/StreamLogger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>


StreamLogger::StreamLogger(std::ostream& os)
: _os {os}
{
}

void
StreamLogger::processLog(const Log& log)
{
	auto now {std::chrono::system_clock::now()};
	auto now_time_t {std::chrono::system_clock::to_time_t(now)};

	_os << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %X") << " [" << getSeverityName(log.getSeverity()) << "] [" << getModuleName(log.getModule()) << "] " << log.getMessage() << std::endl;
}

