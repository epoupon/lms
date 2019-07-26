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

std::string getModuleName(Module mod)
{
	switch (mod)
	{
		case Module::API_SUBSONIC:	return "API_SUBSONIC";
		case Module::AUTH:		return "AUTH";
		case Module::AV:		return "AV";
		case Module::COVER:		return "COVER";
		case Module::DB:		return "DB";
		case Module::DBUPDATER:		return "DB UPDATER";
		case Module::FEATURE:		return "FEATURE";
		case Module::MAIN:		return "MAIN";
		case Module::METADATA:		return "METADATA";
		case Module::REMOTE:		return "REMOTE";
		case Module::SERVICE:		return "SERVICE";
		case Module::SIMILARITY:	return "SIMILARITY";
		case Module::TRANSCODE:		return "TRANSCODE";
		case Module::UI:		return "UI";
	}
	return "";
}

std::string getSeverityName(Severity sev)
{
	switch (sev)
	{
		case Severity::FATAL:		return "fatal";
		case Severity::ERROR:		return "error";
		case Severity::WARNING:		return "warning";
		case Severity::INFO:		return "info";
		case Severity::DEBUG:		return "debug";
	}
	return "";
}

