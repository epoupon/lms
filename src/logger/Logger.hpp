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

#ifndef LOGGER_HPP__
#define LOGGER_HPP__

#include <Wt/WServer>
#include <Wt/WLogger>

#include <string>


enum Severity
{
	SEV_CRIT	= 2,
	SEV_ERROR	= 3,
	SEV_WARNING	= 4,
	SEV_NOTICE	= 5,
	SEV_INFO	= 6,
	SEV_DEBUG	= 7,
};

enum Module
{
	MOD_AV,
	MOD_COVER,
	MOD_DB,
	MOD_DBUPDATER,
	MOD_MAIN,
	MOD_METADATA,
	MOD_REMOTE,
	MOD_SERVICE,
	MOD_TRANSCODE,
	MOD_UI,
};

std::string getModuleName(Module mod);
std::string getSeverityName(Severity sev);

#define LMS_LOG(module, level)	Wt::WServer::instance()->log(getSeverityName(level)) <<  Wt::WLogger::sep << "[" << getModuleName(module) << "]" <<  Wt::WLogger::sep

#endif
