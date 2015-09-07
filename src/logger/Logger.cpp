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
		case MOD_AV:		return "AV";
		case MOD_COVER:		return "COVER";
		case MOD_DB:		return "DB";
		case MOD_DBUPDATER:	return "DB UPDATER";
		case MOD_MAIN:		return "MAIN";
		case MOD_METADATA:	return "METADATA";
		case MOD_REMOTE:	return "REMOTE";
		case MOD_SERVICE:	return "SERVICE";
		case MOD_TRANSCODE:	return "TRANSCODE";
		case MOD_UI:		return "UI";
	}
	return "";
}

std::string getSeverityName(Severity sev)
{
	switch (sev)
	{
		case SEV_CRIT:		return "fatal";
		case SEV_ERROR:		return "error";
		case SEV_WARNING:	return "warning";
		case SEV_NOTICE:
		case SEV_INFO:		return "info";
		case SEV_DEBUG:		return "debug";
	}
	return "";
}

