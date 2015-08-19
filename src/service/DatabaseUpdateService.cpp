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

#include <boost/thread.hpp>

#include "config/ConfigReader.hpp"
#include "logger/Logger.hpp"

#include "DatabaseUpdateService.hpp"

static std::vector<std::string> splitStrings(const std::string& source)
{
	std::vector<std::string> res;
	std::istringstream oss(source);

	std::string str;
	while(oss >> str)
		res.push_back(str);

	return res;
}

namespace Service {

DatabaseUpdateService::DatabaseUpdateService(Wt::Dbo::SqlConnectionPool &connectionPool)
: _metadataParser(),
 _databaseUpdater( connectionPool, _metadataParser)
{
	_databaseUpdater.setAudioExtensions(splitStrings(ConfigReader::instance().getString("main.database.audio_extensions")));
	_databaseUpdater.setVideoExtensions(splitStrings(ConfigReader::instance().getString("main.database.video_extensions")));
}

void
DatabaseUpdateService::start(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, starting...";
	_databaseUpdater.start();
}

void
DatabaseUpdateService::stop(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, stopping...";
	_databaseUpdater.stop();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, stopped";
}

void
DatabaseUpdateService::restart(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, restart";
	stop();
	start();
}

} // namespace Service

