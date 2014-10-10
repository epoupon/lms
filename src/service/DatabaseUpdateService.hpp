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

#ifndef DB_UPDATE_SERVICE_HPP
#define DB_UPDATE_SERVICE_HPP

#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>

#include "metadata/AvFormat.hpp"
#include "database-updater/DatabaseUpdater.hpp"

#include "Service.hpp"

namespace Service {

class DatabaseUpdateService : public Service
{
	public:

		typedef std::shared_ptr<DatabaseUpdateService>	pointer;

		struct Config {
			bool enable;
			boost::filesystem::path dbPath;
			std::vector<std::string>	audioExtensions;
			std::vector<std::string>	videoExtensions;
		};

		DatabaseUpdateService(const Config& config);

		// Service interface
		void start(void);
		void stop(void);
		void restart(void);

	private:

		MetaData::AvFormat			_metadataParser;
		DatabaseUpdater::Updater		_databaseUpdater;	// Todo use handler
};

} // namespace Service

#endif

