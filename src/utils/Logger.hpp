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

#include <string>

#include <Wt/WApplication.h>
#include <Wt/WLogger.h>

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
	COVER,
	DB,
	DBUPDATER,
	FEATURE,
	MAIN,
	METADATA,
	REMOTE,
	SERVICE,
	SIMILARITY,
	TRANSCODE,
	UI,
};

std::string getModuleName(Module mod);
std::string getSeverityName(Severity sev);

#define LMS_LOG(module, level)	Wt::log(getSeverityName(Severity::level)) << Wt::WLogger::sep << "[" << getModuleName(Module::module) << "]" <<  Wt::WLogger::sep

