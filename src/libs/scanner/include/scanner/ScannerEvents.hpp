/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <Wt/WDateTime.h>
#include <Wt/WSignal.h>

#include "ScannerStats.hpp"

namespace Scanner
{

	struct Events
	{
		// Called just after scan start
		Wt::Signal<> 				scanStarted;

		// Called just after scan complete (true if changes have been made)
		Wt::Signal<ScanStats>		scanComplete;

		// Called during scan in progress
		Wt::Signal<ScanStepStats>	scanInProgress;

		// Called after a schedule
		Wt::Signal<Wt::WDateTime>	scanScheduled;
	};

} // ns Scanner

