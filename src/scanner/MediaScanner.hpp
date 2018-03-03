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

#include <boost/asio/deadline_timer.hpp>

#include <Wt/WIOService>
#include <Wt/WSignal>

#include "metadata/TagLibParser.hpp"

#include "database/DatabaseHandler.hpp"

namespace Scanner {

enum class UpdatePeriod {
	Never = 0,
	Daily,
	Weekly,
	Monthly
};

UpdatePeriod getUpdatePeriod(Wt::Dbo::Session& session);
void setUpdatePeriod(Wt::Dbo::Session& session, UpdatePeriod updatePeriod);

boost::posix_time::time_duration getUpdateStartTime(Wt::Dbo::Session& session);
void setUpdateStartTime(Wt::Dbo::Session& session, boost::posix_time::time_duration);

class MediaScanner
{
	public:

		MediaScanner(Wt::Dbo::SqlConnectionPool& connectionPool);

		void start();
		void stop();
		void restart();

		struct Stats
		{
			std::size_t	nbNoChange = 0;		// no change since last scan
			std::size_t	nbScanned = 0;		// total scanned filed
			std::size_t	nbScanErrors = 0;	// cannot scan file
			std::size_t	nbNotImported = 0;	// Scanned, but not imported (criteria not filled)
			std::size_t	nbAdded = 0;		// Added in DB
			std::size_t	nbRemoved = 0;		// removed from DB
			std::size_t	nbUpdated = 0;		// updated file in DB

			std::size_t nbChanges() const { return nbAdded + nbRemoved + nbUpdated;}
		};


		// Called just after track addition
		Wt::Signal<Database::Track::pointer>& addedTrack() { return _sigAddedTrack; }

		// Called just before track removal
		Wt::Signal<Database::Track::pointer>& removedTrack() { return _sigRemovedTrack; }

		// Called just after track modification
		Wt::Signal<Database::Track::pointer>& modifiedTrack() { return _sigModifiedTrack; }

		// Called just after scan complete
		Wt::Signal<Stats>& scanComplete() { return _sigScanComplete; }

	private:

		// Job handling
		void processNextJob();
		void scheduleScan(boost::posix_time::time_duration duration);
		void scheduleScan(boost::posix_time::ptime time);

		// Update database (scheduled callback)
		void process(boost::system::error_code ec);

		void processRootDirectory( boost::filesystem::path rootDirectory, Stats& stats);

		// Helpers
		Database::Artist::pointer getArtist( const boost::filesystem::path& file, const std::string& name, const std::string& MBID);
		Database::Release::pointer getRelease( const boost::filesystem::path& file, const std::string& name, const std::string& MBID);
		std::vector<Database::Cluster::pointer> getClusters( const MetaData::Clusters& names);
		void refreshScanSettings();

		// Audio
		void checkAudioFiles( Stats& stats );
		void checkDuplicatedAudioFiles( Stats& stats );
		void processAudioFile( const boost::filesystem::path& file, Stats& stats);

		// Video
		void checkVideoFiles( Stats& stats );
		void processVideoFile( const boost::filesystem::path& file, Stats& stats);

		bool			_running;
		Wt::WIOService		_ioService;

		Wt::Signal<Stats>	_sigScanComplete;
		Wt::Signal<Database::Track::pointer> _sigModifiedTrack;
		Wt::Signal<Database::Track::pointer> _sigAddedTrack;
		Wt::Signal<Database::Track::pointer> _sigRemovedTrack;

		boost::asio::deadline_timer _scheduleTimer;

		Database::Handler	_db;

		// Scan settings
		std::vector<boost::filesystem::path>	_fileExtensions;
		std::vector<boost::filesystem::path>	_rootDirectories;

		MetaData::TagLibParser 	_metadataParser;

}; // class MediaScanner

} // Scanner

