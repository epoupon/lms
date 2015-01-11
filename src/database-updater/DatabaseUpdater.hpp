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

#ifndef DB_UPDATER_HPP
#define DB_UPDATER_HPP

#include <boost/asio/deadline_timer.hpp>
#include <Wt/WIOService>

#include "metadata/MetaData.hpp"

#include "database/DatabaseHandler.hpp"

namespace DatabaseUpdater {

class Updater
{
	public:
		Updater(boost::filesystem::path db, MetaData::Parser& parser);

		void setAudioExtensions(const std::vector<std::string>&	extensions);
		void setVideoExtensions(const std::vector<std::string>&	extensions);

		void start();
		void stop();

	private:

		struct Stats
		{
			std::size_t	nbAdded;
			std::size_t	nbRemoved;
			std::size_t	nbModified;
			Stats() : nbAdded(0), nbRemoved(0), nbModified(0) {}

			void	clear(void) { nbAdded = 0; nbRemoved = 0; nbModified = 0; }
			std::size_t nbChanges() const { return nbAdded + nbRemoved + nbModified;}
		};

		// Job handling
		void processNextJob();
		void scheduleScan(boost::posix_time::time_duration duration);
		void scheduleScan(boost::posix_time::ptime time);

		// Update database (scheduled callback)
		void process(boost::system::error_code ec);

		// Check if a file exists and is still in a root directory
		static bool checkFile(const boost::filesystem::path& p,
				const std::vector<boost::filesystem::path>& rootDirectories,
				const std::vector<boost::filesystem::path>& extensions);


		void processDirectory(  const boost::filesystem::path& rootDirectory,
					const boost::filesystem::path& directory,
					Database::MediaDirectory::Type type,
					Stats& stats);


		// Audio
		void checkAudioFiles( Stats& stats );
		void processAudioFile( const boost::filesystem::path& file, Stats& stats);

		// Video
		void checkVideoFiles( Stats& stats );
		void processVideoFile( const boost::filesystem::path& file, Stats& stats);

		bool			_running;
		Wt::WIOService		_ioService;

		boost::asio::deadline_timer _scheduleTimer;

		Database::Handler	_db;

		std::vector<boost::filesystem::path>	_audioExtensions;
		std::vector<boost::filesystem::path>	_videoExtensions;

		MetaData::Parser&	_metadataParser;


}; // class Updater

} // DatabaseUpdater

#endif
