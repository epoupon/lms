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

#include "utils/EnumSet.hpp"
#include "utils/Logger.hpp"

class StreamLogger final : public Logger
{
	public:
		static constexpr EnumSet<Severity> defaultSeverities {Severity::FATAL, Severity::ERROR, Severity::WARNING, Severity::INFO};

		StreamLogger(std::ostream& oss, EnumSet<Severity> severities = defaultSeverities);

		void processLog(const Log& log);

	private:
		std::ostream& _os;
		const EnumSet<Severity> _severities;
};

