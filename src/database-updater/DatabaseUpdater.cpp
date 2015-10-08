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


#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/asio/placeholders.hpp>

#include "logger/Logger.hpp"

#include "database/Types.hpp"

#include "Checksum.hpp"
#include "DatabaseUpdater.hpp"
#include "cover/CoverArtGrabber.hpp"

namespace {

boost::gregorian::date
getNextDay(const boost::gregorian::date& current)
{
	boost::gregorian::day_iterator it(current);
	return *(++it);
}

boost::gregorian::date
getNextMonday(const boost::gregorian::date& current)
{
	boost::gregorian::day_iterator it(current);

	++it;
	// While it's not monday
	while( it->day_of_week() != 1 )
		++it;

	return *(it);
}

boost::gregorian::date
getNextFirstOfMonth(const boost::gregorian::date& current)
{
	boost::gregorian::day_iterator it(current);

	++it;
	// While it's not the 1st of the month
	while( it->day() != 1 )
		++it;

	return (*it);
}


bool
isFileSupported(const boost::filesystem::path& file, const std::vector<boost::filesystem::path> extensions)
{

	boost::filesystem::path fileExtension = file.extension();

	for (auto& extension : extensions)
	{
		if (extension == fileExtension)
			return true;
	}

	return false;
}

std::vector<boost::filesystem::path>
getRootDirectoriesByType(Wt::Dbo::Session& session, Database::MediaDirectory::Type type)
{
	Wt::Dbo::Transaction transaction(session);

	std::vector<boost::filesystem::path> res;
	std::vector<Database::MediaDirectory::pointer> rootDirs = Database::MediaDirectory::getByType(session, type);

	for (auto rootDir : rootDirs)
		res.push_back(rootDir->getPath());

	return res;
}

bool
isPathInParentPath(const boost::filesystem::path& path, const boost::filesystem::path& parentPath)
{
	boost::filesystem::path curPath = path;

	while (curPath.has_parent_path())
	{
		curPath = curPath.parent_path();

		if (curPath == parentPath)
			return true;
	}

	return false;
}

} // namespace


namespace DatabaseUpdater {

using namespace Database;


Updater::Updater(Wt::Dbo::SqlConnectionPool &connectionPool, MetaData::Parser& parser)
 : _running(false),
_scheduleTimer(_ioService),
_db(connectionPool),
_metadataParser(parser)
{
	_ioService.setThreadCount(1);
}

void
Updater::setAudioExtensions(const std::vector<std::string>& extensions)
{
	for (const std::string& extension : extensions)
		_audioExtensions.push_back("." + extension);
}

void
Updater::setVideoExtensions(const std::vector<std::string>& extensions)
{
	for (const std::string& extension : extensions)
		_videoExtensions.push_back("." + extension);
}

void
Updater::start(void)
{
	_running = true;

	// post some jobs in the io_service
	processNextJob();

	_ioService.start();
}

void
Updater::stop(void)
{
	_running = false;

	// TODO cancel all jobs (timer, ...)
	_scheduleTimer.cancel();

	_ioService.stop();
}

void
Updater::processNextJob(void)
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	MediaDirectorySettings::pointer settings = MediaDirectorySettings::get(_db.getSession());

	if (settings->getManualScanRequested()) {
		LMS_LOG(DBUPDATER, INFO) << "Manual scan requested!";
		scheduleScan( boost::posix_time::seconds(0) );
	}
	else
	{
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		boost::posix_time::time_duration startTime = settings->getUpdateStartTime();

		boost::gregorian::date nextScanDate;

		switch( settings->getUpdatePeriod() )
		{
			case Database::MediaDirectorySettings::Never:
				// Nothing to do
				break;
			case Database::MediaDirectorySettings::Daily:
				if (now.time_of_day() < startTime)
					nextScanDate = now.date();
				else
					nextScanDate = getNextDay(now.date());
				break;
			case Database::MediaDirectorySettings::Weekly:
				if (now.time_of_day() < startTime && now.date().day_of_week() == 1)
					nextScanDate = now.date();
				else
					nextScanDate = getNextMonday(now.date());
				break;
			case Database::MediaDirectorySettings::Monthly:
				if (now.time_of_day() < startTime && now.date().day() == 1)
					nextScanDate = now.date();
				else
					nextScanDate = getNextFirstOfMonth(now.date());
				break;
		}

		if (!nextScanDate.is_special())
			scheduleScan( boost::posix_time::ptime (nextScanDate, settings->getUpdateStartTime() ) );
	}
}

void
Updater::scheduleScan( boost::posix_time::time_duration duration)
{
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan in " << duration;
	_scheduleTimer.expires_from_now(duration);
	_scheduleTimer.async_wait( boost::bind( &Updater::process, this, boost::asio::placeholders::error) );
}

void
Updater::scheduleScan( boost::posix_time::ptime time)
{
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan at " << time;
	_scheduleTimer.expires_at(time);
	_scheduleTimer.async_wait( boost::bind( &Updater::process, this, boost::asio::placeholders::error) );
}

void
Updater::process(boost::system::error_code err)
{
	if (!err)
	{
		Stats stats;

		checkAudioFiles(stats);
		checkVideoFiles(stats);

		std::vector<RootDirectory> rootDirectories;
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			for (MediaDirectory::pointer directory : MediaDirectory::getAll(_db.getSession()))
				rootDirectories.push_back( RootDirectory( directory->getType(), directory->getPath() ));
		}

		for (RootDirectory rootDirectory : rootDirectories)
		{
			if (!_running)
				break;

			LMS_LOG(DBUPDATER, INFO) << "Processing root directory '" << rootDirectory.path << "'...";
			processRootDirectory(rootDirectory, stats);
			LMS_LOG(DBUPDATER, INFO) << "Processing root directory '" << rootDirectory.path << "' DONE";
		}

		LMS_LOG(DBUPDATER, INFO) << "Scan complete. Changes = " << stats.nbChanges() << ", Errors = " << stats.nbScanErrors;

		// Update database stats
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Database::MediaDirectorySettings::pointer settings = Database::MediaDirectorySettings::get(_db.getSession());

			if (stats.nbChanges() > 0)
				settings.modify()->setLastUpdate(now);

			// Save the last scan only if it has been completed
			if (_running)
				settings.modify()->setLastScan(now);

			// If the manual scan was required we can now set it to done
			// Update only if the scan is complete!
			if (settings->getManualScanRequested() && _running)
				settings.modify()->setManualScanRequested(false);

		}

		if (_running)
			processNextJob();
	}
}

Artist::pointer
Updater::getArtist( const boost::filesystem::path& file, const std::string& name, const std::string& mbid)
{
	Artist::pointer artist;

	// First try to get by MBID
	if (!mbid.empty())
	{
		artist = Artist::getByMBID( _db.getSession(), mbid );
		if (!artist)
			artist = Artist::create( _db.getSession(), name, mbid);

		return artist;
	}

	// Fall back on artist name (collisions may occur)
	if (!name.empty())
	{
		for (Artist::pointer sameNamedArtist : Artist::getByName( _db.getSession(), name ))
		{
			if (sameNamedArtist->getMBID().empty())
			{
				artist = sameNamedArtist;
				break;
			}
		}

		// No Artist found with the same name and without MBID -> creating
		if (!artist)
			artist = Artist::create( _db.getSession(), name);

		return artist;
	}

	return Artist::getNone( _db.getSession() );
}

Release::pointer
Updater::getRelease( const boost::filesystem::path& file, const std::string& name, const std::string& mbid)
{
	Release::pointer release;

	// First try to get by MBID
	if (!mbid.empty())
	{
		release = Release::getByMBID( _db.getSession(), mbid );
		if (!release)
			release = Release::create( _db.getSession(), name, mbid);

		return release;
	}

	// Fall back on release name (collisions may occur)
	if (!name.empty())
	{
		for (Release::pointer sameNamedRelease : Release::getByName( _db.getSession(), name ))
		{
			if (sameNamedRelease->getMBID().empty())
			{
				release = sameNamedRelease;
				break;
			}
		}

		// No release found with the same name and without MBID -> creating
		if (!release)
			release = Release::create( _db.getSession(), name);

		return release;
	}

	return Release::getNone( _db.getSession() );
}

std::vector<Genre::pointer>
Updater::getGenres( const std::list<std::string>& names)
{
	std::vector< Genre::pointer > genres;

	for (const std::string& name : names)
	{
		Genre::pointer genre ( Genre::getByName(_db.getSession(), name) );
		if (!genre)
			genre = Genre::create(_db.getSession(), name);

		genres.push_back( genre );
	}

	if (genres.empty())
		genres.push_back( Genre::getNone( _db.getSession() ));

	return genres;
}

void
Updater::processAudioFile( const boost::filesystem::path& file, Stats& stats)
{
	boost::posix_time::ptime lastWriteTime (boost::posix_time::from_time_t( boost::filesystem::last_write_time( file ) ) );

	// Skip file if last write is the same
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		Wt::Dbo::ptr<Track> track = Track::getByPath(_db.getSession(), file);

		if (track && track->getLastWriteTime() == lastWriteTime)
			return;
	}

	MetaData::Items items;
	if (!_metadataParser.parse(file, items))
		return;

	Wt::Dbo::Transaction transaction(_db.getSession());

	Wt::Dbo::ptr<Track> track = Track::getByPath(_db.getSession(), file);

	// We estimate this is a audio file if:
	// - we found a least one audio stream
	// - the duration is not null
	if (items.find(MetaData::Type::AudioStreams) == items.end()
			|| boost::any_cast<std::vector<MetaData::AudioStream> >(items[MetaData::Type::AudioStreams]).empty())
	{
		LMS_LOG(DBUPDATER, DEBUG) << "Skipped '" << file << "' (no audio stream found)";

		// If Track exists here, delete it!
		if (track)
		{
			track.remove();
			stats.nbRemoved++;
		}
		return;
	}
	if (items.find(MetaData::Type::Duration) == items.end()
			|| boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Type::Duration]).total_seconds() <= 0)
	{
		LMS_LOG(DBUPDATER, DEBUG) << "Skipped '" << file << "' (no duration or duration <= 0)";

		// If Track exists here, delete it!
		if (track)
		{
			track.remove();
			stats.nbRemoved++;
		}
		return;
	}

	// ***** Title
	std::string title;
	if (items.find(MetaData::Type::Title) != items.end()) {
			title = boost::any_cast<std::string>(items[MetaData::Type::Title]);
		}
		else
		{
			// TODO parse file name guess track etc.
			// For now juste use file name as title
			title = file.filename().string();
		}


		// ***** Genres
		std::vector< Genre::pointer > genres;
		{
			std::list<std::string> genreList;

			if (items.find(MetaData::Type::Genres) != items.end())
				genreList = boost::any_cast< std::list<std::string> > (items[MetaData::Type::Genres]);

			genres = getGenres( genreList );
		}
		assert( !genres.empty() );

		//  ***** Artist
		Artist::pointer artist;
		{
			std::string artistName;
			std::string artistMusicBrainzID;

			if (items.find(MetaData::Type::MusicBrainzArtistID) != items.end())
				artistMusicBrainzID = boost::any_cast<std::string>(items[MetaData::Type::MusicBrainzArtistID] );

			if (items.find(MetaData::Type::Artist) != items.end())
				artistName = boost::any_cast<std::string>(items[MetaData::Type::Artist]);

			artist = getArtist(file, artistName, artistMusicBrainzID);
		}
		assert(artist);

		//  ***** Release
		Release::pointer release;
		{
			std::string releaseName;
			std::string releaseMusicBrainzID;

			if (items.find(MetaData::Type::MusicBrainzAlbumID) != items.end())
				releaseMusicBrainzID = boost::any_cast<std::string>(items[MetaData::Type::MusicBrainzAlbumID] );

			if (items.find(MetaData::Type::Album) != items.end())
				releaseName = boost::any_cast<std::string>(items[MetaData::Type::Album]);

			release = getRelease(file, releaseName, releaseMusicBrainzID);
		}
		assert(release);

		// If file already exist, update data
		// Otherwise, create it
		if (!track)
		{
			// Create a new song
			track = Track::create(_db.getSession(), file);
			LMS_LOG(DBUPDATER, INFO) << "Adding '" << file << "'";
			stats.nbAdded++;
		}
		else
		{
			LMS_LOG(DBUPDATER, INFO) << "Updating '" << file << "'";
			stats.nbModified++;
		}

		assert(track);

		track.modify()->setArtist(artist);
		track.modify()->setRelease(release);
		track.modify()->setLastWriteTime(lastWriteTime);
		track.modify()->setName(title);
		track.modify()->setDuration( boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Type::Duration]) );
		track.modify()->setAddedTime( boost::posix_time::second_clock::local_time() );

		{
			std::string trackGenreList;
			// Product genre list
			for (Genre::pointer genre : genres)
			{
				if (!trackGenreList.empty())
					trackGenreList += ", ";
				trackGenreList += genre->getName();
			}

			track.modify()->setGenres( trackGenreList );
		}
		track.modify()->setGenres( genres );

		if (items.find(MetaData::Type::TrackNumber) != items.end())
			track.modify()->setTrackNumber( boost::any_cast<std::size_t>(items[MetaData::Type::TrackNumber]) );

		if (items.find(MetaData::Type::DiscNumber) != items.end())
			track.modify()->setDiscNumber( boost::any_cast<std::size_t>(items[MetaData::Type::DiscNumber]) );

		if (items.find(MetaData::Type::Date) != items.end())
			track.modify()->setDate( boost::any_cast<boost::posix_time::ptime>(items[MetaData::Type::Date]) );

		if (items.find(MetaData::Type::OriginalDate) != items.end())
		{
			track.modify()->setOriginalDate( boost::any_cast<boost::posix_time::ptime>(items[MetaData::Type::OriginalDate]) );

			// If a file has an OriginalDate but no date, set the date to ease filtering
			if (items.find(MetaData::Type::Date) == items.end())
				track.modify()->setDate( boost::any_cast<boost::posix_time::ptime>(items[MetaData::Type::OriginalDate]) );
		}

		if (items.find(MetaData::Type::HasCover) != items.end())
		{
			bool hasCover = boost::any_cast<bool>(items[MetaData::Type::HasCover]);

			track.modify()->setCoverType( hasCover ? Track::CoverType::Embedded : Track::CoverType::None );
		}

		transaction.commit();
}


void
Updater::processRootDirectory(RootDirectory rootDirectory, Stats& stats)
{

	if (!boost::filesystem::exists(rootDirectory.path) || !boost::filesystem::is_directory(rootDirectory.path))
		return;

	boost::filesystem::recursive_directory_iterator itPath(rootDirectory.path);
	boost::filesystem::recursive_directory_iterator itEnd;
	while (itPath != itEnd)
	{
		if (!_running)
			return;

		if (boost::filesystem::is_regular(*itPath))
		{
			switch( rootDirectory.type )
			{
				case Database::MediaDirectory::Audio:
					if (isFileSupported(*itPath, _audioExtensions))
						processAudioFile( *itPath, stats );

					break;

				case Database::MediaDirectory::Video:
					if (isFileSupported(*itPath, _videoExtensions))
						processVideoFile( *itPath, stats);
					break;
			}
		}

		++itPath;
	}
}

bool
Updater::checkFile(const boost::filesystem::path& p, const std::vector<boost::filesystem::path>& rootDirs, const std::vector<boost::filesystem::path>& extensions)
{
	try
	{
		bool status = true;

		// For each track, make sure the the file still exists
		// and still belongs to a root directory
		if (!boost::filesystem::exists( p )
				|| !boost::filesystem::is_regular( p ) )
		{
			LMS_LOG(DBUPDATER, INFO) << "Missing file '" << p << "'";
			status = false;
		}
		else
		{

			bool foundRoot = false;
			for (auto& rootDir : rootDirs)
			{
				if (isPathInParentPath(p, rootDir))
				{
					foundRoot = true;
					break;
				}
			}

			if (!foundRoot)
			{
				LMS_LOG(DBUPDATER, INFO) << "Out of root file '" << p << "'";
				status = false;
			}
			else if (!isFileSupported(p, extensions))
			{
				LMS_LOG(DBUPDATER, INFO) << "File format no longer supported for '" << p << "'";
				status = false;
			}
		}

		return status;

	}
	catch (boost::filesystem::filesystem_error& e)
	{
		LMS_LOG(DBUPDATER, ERROR) << "Caught exception while checking file '" << p << "': " << e.what();
		return false;
	}

}

void
Updater::checkAudioFiles( Stats& stats )
{
	LMS_LOG(DBUPDATER, INFO) << "Checking audio files...";

	std::vector<boost::filesystem::path> trackPaths = Track::getAllPaths(_db.getSession());;
	std::vector<boost::filesystem::path> rootDirs = getRootDirectoriesByType(_db.getSession(), Database::MediaDirectory::Audio);

	LMS_LOG(DBUPDATER, DEBUG) << "Checking tracks...";
	for (auto& trackPath : trackPaths)
	{
		if (!_running)
			return;

		if (!checkFile(trackPath, rootDirs, _audioExtensions))
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Track::pointer track = Track::getByPath(_db.getSession(), trackPath);
			if (track)
			{
				track.remove();
				stats.nbRemoved++;
			}
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking Genres...";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		// Now process orphan Genre (no track)
		auto genres = Genre::getAll(_db.getSession());
		for (auto genre : genres)
		{
			if (genre->getTracks().size() == 0)
			{
				LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan genre '" << genre->getName() << "'";
				genre.remove();
			}
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking artists...";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		auto artists = Artist::getAllOrphans(_db.getSession());
		for (auto artist : artists)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan artist '" << artist->getName() << "'";
			artist.remove();
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking releases...";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		auto releases = Release::getAllOrphans(_db.getSession());
		for (auto release : releases)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan release '" << release->getName() << "'";
			release.remove();
		}
	}

	LMS_LOG(DBUPDATER, INFO) << "Check audio files done!";
}

void
Updater::checkVideoFiles( Stats& stats )
{
	std::vector<boost::filesystem::path> rootDirs = getRootDirectoriesByType(_db.getSession(), Database::MediaDirectory::Video);
	std::vector<boost::filesystem::path> videoPaths = Video::getAllPaths(_db.getSession());

	LMS_LOG(DBUPDATER, DEBUG) << "Checking videos...";
	for (auto& videoPath : videoPaths)
	{
		if (!_running)
			return;

		if (!checkFile(videoPath, rootDirs, _videoExtensions))
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Video::pointer video = Video::getByPath(_db.getSession(), videoPath);
			if (video)
			{
				video.remove();
				stats.nbRemoved++;
			}
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Check video files done!";
}
void
Updater::processVideoFile( const boost::filesystem::path& file, Stats& stats)
{
	try {

		// Check last update time
	        boost::posix_time::ptime lastWriteTime (boost::posix_time::from_time_t( boost::filesystem::last_write_time( file ) ) );

		Wt::Dbo::Transaction transaction(_db.getSession());

		// Skip file if last write is the same
		Wt::Dbo::ptr<Video> video = Video::getByPath(_db.getSession(), file);
		if (video && video->getLastWriteTime() == lastWriteTime)
			return;

		MetaData::Items items;
		_metadataParser.parse(file, items);

		// We estimate this is a video if:
		// - we found a least one video stream
		// - the duration is not null
		if (items.find(MetaData::Type::VideoStreams) == items.end()
		|| boost::any_cast<std::vector<MetaData::VideoStream> >(items[MetaData::Type::VideoStreams]).empty())
		{
			LMS_LOG(DBUPDATER, ERROR) << "Skipped '" << file << "' (no video stream found)";

			// If the video exists here, delete it!
			if (video) {
				video.remove();
				stats.nbRemoved++;
			}
			return;
		}
		if (items.find(MetaData::Type::Duration) == items.end()
		|| boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Type::Duration]).total_seconds() == 0)
		{
			LMS_LOG(DBUPDATER, ERROR) << "Skipped '" << file << "' (no duration or duration 0)";

			// If Track exists here, delete it!
			if (video) {
				video.remove();
				stats.nbRemoved++;
			}
			return;
		}

		// If video already exist, update data
		// Otherwise, create it
		// Today we are very aggressive, but we could also guess names from path, etc.
		if (!video)
		{
			video = Video::create(_db.getSession(), file);
			LMS_LOG(DBUPDATER, DEBUG) << "Adding '" << file << "'";
			stats.nbAdded++;
		}
		else
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Updating '" << file << "'";
			stats.nbModified++;
		}

		assert(video);

		video.modify()->setName( file.filename().string() );
		video.modify()->setDuration( boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Type::Duration]) );
		video.modify()->setLastWriteTime(lastWriteTime);

		transaction.commit();
	}
	catch( std::exception& e )
	{
		LMS_LOG(DBUPDATER, ERROR) << "Exception while parsing video file : '" << file << "': '" << e.what() << "' => skipping!";
		stats.nbScanErrors++;
	}
}

} // namespace DatabaseUpdater
