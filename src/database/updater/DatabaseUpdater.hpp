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

#include <mutex>
#include <list>

#include <boost/asio/deadline_timer.hpp>

#include <Wt/WIOService>
#include <Wt/WSignal>

#include "metadata/TagLibParser.hpp"

#include "database/DatabaseHandler.hpp"

namespace Database {


class UpdaterEventHandler;

class Updater
{
	public:

		static Updater& instance();

		void setConnectionPool(Wt::Dbo::SqlConnectionPool& connectionPool);

		void setAudioExtensions(const std::vector<std::string>&	extensions);
		void setVideoExtensions(const std::vector<std::string>&	extensions);

		void start();
		void stop();
		void restart();

		struct Stats
		{
			std::size_t	nbSkipped = 0;		// no change since last scan
			std::size_t	nbScanned = 0;
			std::size_t	nbScanErrors = 0;	// cannot scan file
			std::size_t	nbNotImported = 0;	// Not imported (criteria not filled)
			std::size_t	nbAdded = 0;
			std::size_t	nbRemoved = 0;
			std::size_t	nbModified = 0;

			std::size_t nbChanges() const { return nbAdded + nbRemoved + nbModified;}
		};

		// Emitted when the whole database has been scanned (and all the event handlers have been called)
		Wt::Signal<Stats>& scanComplete() { return _sigScanComplete; }

		// Emitted when a track changed
		// true -> added or modified, false -> to be deleted
		// id of the track
		// musicbrainz recordid
		// path of the track
		typedef Wt::Signal<bool, Track::id_type, std::string, boost::filesystem::path> SigTrackChanged;
		SigTrackChanged& trackChanged() { return _sigTrackChanged; }

		std::mutex&	getMutex(void) { return _mutex; }

		Database::Handler& getDb(void) { return *_db; }

		bool quitRequested(void) const { return !_running;}

		void registerEventHandler(std::shared_ptr<UpdaterEventHandler> handler) { _eventHandlers.push_back(handler); }

	private:

		Updater();

		struct RootDirectory
		{
			Database::MediaDirectory::Type type;
			boost::filesystem::path path;

			RootDirectory(Database::MediaDirectory::Type t, boost::filesystem::path p) : type(t), path(p) {}
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


		void processRootDirectory(  RootDirectory rootDirectory, Stats& stats);

		// Helpers
		Database::Artist::pointer getArtist( const boost::filesystem::path& file, const std::string& name, const std::string& MBID);
		Database::Release::pointer getRelease( const boost::filesystem::path& file, const std::string& name, const std::string& MBID);
		std::vector<Database::Cluster::pointer> getGenreClusters( const std::list<std::string>& names);
		void updateFileExtensions();

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
		Wt::Signal<bool, Artist::id_type>	_sigArtistChanged;
		Wt::Signal<bool, Release::id_type>	_sigReleaseChanged;
		SigTrackChanged				_sigTrackChanged;
		std::mutex		_mutex;

		boost::asio::deadline_timer _scheduleTimer;

		Database::Handler*	_db = nullptr;

		std::vector<boost::filesystem::path>	_audioFileExtensions;
		std::vector<boost::filesystem::path>	_videoFileExtensions;

		MetaData::TagLibParser 	_metadataParser;

		std::list<std::shared_ptr<UpdaterEventHandler> > _eventHandlers;

}; // class Updater

// Helper to get the updater session data
static inline Wt::Dbo::Session& UpdaterDboSession()
{
	return Updater::instance().getDb().getSession();
}

static inline bool UpdaterQuitRequested()
{
	return Updater::instance().quitRequested();
}


class UpdaterEventHandler
{
	public:

		// called when all the files have been scanned by the updater
		virtual void handleFilesUpdated(void) = 0;

	private:

};

} // Database

