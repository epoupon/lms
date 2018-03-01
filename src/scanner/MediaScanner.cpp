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

#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/asio/placeholders.hpp>

#include "cover/CoverArtGrabber.hpp"

#include "database/Setting.hpp"
#include "database/Types.hpp"

#include "utils/Logger.hpp"
#include "utils/Path.hpp"
#include "utils/Utils.hpp"

#include "MediaScanner.hpp"

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


namespace Scanner {

using namespace Database;

MediaScanner::MediaScanner(Wt::Dbo::SqlConnectionPool& connectionPool)
 : _running(false),
_scheduleTimer(_ioService),
_db(connectionPool)
{
	_ioService.setThreadCount(1);

	Wt::Dbo::Transaction transaction(_db.getSession());

	if (!Setting::exists(_db.getSession(), "file_extensions"))
		Setting::setString(_db.getSession(), "file_extensions", ".mp3 .ogg .oga .aac .m4a .flac .wav .wma .aif .aiff .ape .mpc .shn" );
}

void
MediaScanner::restart(void)
{
	stop();
	start();
}

void
MediaScanner::start(void)
{
	_running = true;

	// post some jobs in the io_service
	processNextJob();

	_ioService.start();
}

void
MediaScanner::stop(void)
{
	_running = false;

	_scheduleTimer.cancel();

	_ioService.stop();
}

void
MediaScanner::processNextJob(void)
{
	if (Setting::getBool(_db.getSession(), "manual_scan_requested", false))
	{
		LMS_LOG(DBUPDATER, INFO) << "Manual scan requested!";
		scheduleScan( boost::posix_time::seconds(0) );
	}
	else
	{
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		boost::posix_time::time_duration startTime = Setting::getDuration(_db.getSession(), "update_start_time");

		boost::gregorian::date nextScanDate;

		std::string updatePeriod = Setting::getString(_db.getSession(), "update_period", "never");
		if (updatePeriod == "daily")
		{
			if (now.time_of_day() < startTime)
				nextScanDate = now.date();
			else
				nextScanDate = getNextDay(now.date());
		}
		else if (updatePeriod == "weekly")
		{
			if (now.time_of_day() < startTime && now.date().day_of_week() == 1)
				nextScanDate = now.date();
			else
				nextScanDate = getNextMonday(now.date());
		}
		else if (updatePeriod == "monthly")
		{
			if (now.time_of_day() < startTime && now.date().day() == 1)
				nextScanDate = now.date();
			else
				nextScanDate = getNextFirstOfMonth(now.date());
		}

		if (!nextScanDate.is_special())
			scheduleScan( boost::posix_time::ptime (nextScanDate, startTime) );
	}
}

void
MediaScanner::scheduleScan( boost::posix_time::time_duration duration)
{
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan in " << duration;
	_scheduleTimer.expires_from_now(duration);
	_scheduleTimer.async_wait( boost::bind( &MediaScanner::process, this, boost::asio::placeholders::error) );
}

void
MediaScanner::scheduleScan( boost::posix_time::ptime time)
{
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan at " << time;
	_scheduleTimer.expires_at(time);
	_scheduleTimer.async_wait( boost::bind( &MediaScanner::process, this, boost::asio::placeholders::error) );
}

void
MediaScanner::process(boost::system::error_code err)
{
	if (err)
		return;

	refreshScanSettings();

	Stats stats;

	checkAudioFiles(stats);

	for (auto rootDirectory : _rootDirectories)
	{
		if (!_running)
			break;

		LMS_LOG(DBUPDATER, INFO) << "Processing root directory '" << rootDirectory << "'...";
		processRootDirectory(rootDirectory, stats);
		LMS_LOG(DBUPDATER, INFO) << "Processing root directory '" << rootDirectory << "' DONE";
	}

	if (_running)
		checkDuplicatedAudioFiles(stats);

	LMS_LOG(DBUPDATER, INFO) << "Scan " << (_running ? "complete" : "aborted") << ". Changes = " << stats.nbChanges() << " (added = " << stats.nbAdded << ", removed = " << stats.nbRemoved << ", updated = " << stats.nbUpdated << "), Not changed = " << stats.nbNoChange << ", Scanned = " << stats.nbScanned << " (errors = " << stats.nbScanErrors << ", not imported = " << stats.nbNotImported << ")";

	// Update database stats
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	if (stats.nbChanges() > 0)
		Setting::setTime(_db.getSession(), "last_update", now);

	// Save the last scan only if it has been completed
	if (_running)
	{
		Setting::setTime(_db.getSession(), "last_scan", now);
		Setting::setBool(_db.getSession(), "manual_scan_requested", false);

		processNextJob();

		scanComplete().emit(stats);
	}
}

void
MediaScanner::refreshScanSettings()
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	_fileExtensions.clear();
	for (auto extension : splitString(Setting::getString(_db.getSession(), "file_extensions"), " "))
		_fileExtensions.push_back( extension );

	_rootDirectories.clear();
	for (auto rootDir : Database::MediaDirectory::getAll(_db.getSession()))
		_rootDirectories.push_back(rootDir->getPath());
}

Artist::pointer
MediaScanner::getArtist( const boost::filesystem::path& file, const std::string& name, const std::string& mbid)
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

	return Artist::pointer();
}

Release::pointer
MediaScanner::getRelease( const boost::filesystem::path& file, const std::string& name, const std::string& mbid)
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

	return Release::pointer();
}

std::vector<Cluster::pointer>
MediaScanner::getClusters( const MetaData::Clusters& clustersNames)
{
	std::vector< Cluster::pointer > clusters;

	for (auto clusterNames : clustersNames)
	{
		ClusterType::pointer clusterType = ClusterType::getByName(_db.getSession(), clusterNames.first);
		if (!clusterType)
			clusterType = ClusterType::create(_db.getSession(), clusterNames.first);

		for (auto clusterName : clusterNames.second)
		{
			Cluster::pointer cluster = clusterType->getCluster(clusterName);
			if (!cluster)
				cluster = Cluster::create(_db.getSession(), clusterType, clusterName);

			clusters.push_back(cluster);
		}
	}

	return clusters;
}

void
MediaScanner::processAudioFile( const boost::filesystem::path& file, Stats& stats)
{
	boost::posix_time::ptime lastWriteTime (boost::posix_time::from_time_t( boost::filesystem::last_write_time( file ) ) );

	// Skip file if last write is the same
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		Wt::Dbo::ptr<Track> track = Track::getByPath(_db.getSession(), file);

		if (track && track->getLastWriteTime() == lastWriteTime)
		{
			stats.nbNoChange++;
			return;
		}
	}

	boost::optional<MetaData::Items> items = _metadataParser.parse(file);
	if (!items)
	{
		stats.nbScanErrors++;
		return;
	}

	stats.nbScanned++;

	std::vector<unsigned char> checksum ;
	computeCrc(file, checksum);

	Wt::Dbo::Transaction transaction(_db.getSession());

	Wt::Dbo::ptr<Track> track = Track::getByPath(_db.getSession(), file);

	// We estimate this is a audio file if:
	// - we found a least one audio stream
	// - the duration is not null
	if ((*items).find(MetaData::Type::AudioStreams) == (*items).end()
			|| boost::any_cast<std::vector<MetaData::AudioStream>> ((*items)[MetaData::Type::AudioStreams]).empty())
	{
		LMS_LOG(DBUPDATER, INFO) << "Skipped '" << file << "' (no audio stream found)";

		// If Track exists here, delete it!
		if (track)
		{
			track.remove();
			stats.nbRemoved++;
		}
		stats.nbNotImported++;
		return;
	}
	if ((*items).find(MetaData::Type::Duration) == (*items).end()
			|| boost::any_cast<boost::posix_time::time_duration>((*items)[MetaData::Type::Duration]).total_seconds() <= 0)
	{
		LMS_LOG(DBUPDATER, INFO) << "Skipped '" << file << "' (no duration or duration <= 0)";

		// If Track exists here, delete it!
		if (track)
		{
			track.remove();
			stats.nbRemoved++;
		}
		stats.nbNotImported++;
		return;
	}

	// ***** Title
	std::string title;
	if ((*items).find(MetaData::Type::Title) != (*items).end())
	{
		title = boost::any_cast<std::string>((*items)[MetaData::Type::Title]);
	}
	else
	{
		// TODO parse file name guess track etc.
		// For now juste use file name as title
		title = file.filename().string();
	}

	// ***** Clusters
	std::vector< Cluster::pointer > genres;
	{
		MetaData::Clusters clusterNames;

		if ((*items).find(MetaData::Type::Clusters) != (*items).end())
		{
			clusterNames = boost::any_cast<MetaData::Clusters> ((*items)[MetaData::Type::Clusters]);
		}

		// TODO rename
		genres = getClusters( clusterNames );
	}

	//  ***** Artist
	Artist::pointer artist;
	{
		std::string artistName;
		std::string artistMusicBrainzID;

		if ((*items).find(MetaData::Type::MusicBrainzArtistID) != (*items).end())
			artistMusicBrainzID = boost::any_cast<std::string>((*items)[MetaData::Type::MusicBrainzArtistID] );

		if ((*items).find(MetaData::Type::Artist) != (*items).end())
			artistName = boost::any_cast<std::string>((*items)[MetaData::Type::Artist]);

		artist = getArtist(file, artistName, artistMusicBrainzID);
	}

	//  ***** Release
	Release::pointer release;
	{
		std::string releaseName;
		std::string releaseMusicBrainzID;

		if ((*items).find(MetaData::Type::MusicBrainzAlbumID) != (*items).end())
			releaseMusicBrainzID = boost::any_cast<std::string>((*items)[MetaData::Type::MusicBrainzAlbumID] );

		if ((*items).find(MetaData::Type::Album) != (*items).end())
			releaseName = boost::any_cast<std::string>((*items)[MetaData::Type::Album]);

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

		// Remove the songs from its clusters
		for (auto cluster : track->getClusters())
			cluster.remove();

		stats.nbUpdated++;
	}

	assert(track);

	track.modify()->setChecksum(checksum);
	track.modify()->setArtist(artist);
	track.modify()->setRelease(release);
	track.modify()->setLastWriteTime(lastWriteTime);
	track.modify()->setName(title);
	track.modify()->setDuration( boost::any_cast<boost::posix_time::time_duration>((*items)[MetaData::Type::Duration]) );
	track.modify()->setAddedTime( boost::posix_time::second_clock::local_time() );

	{
		std::string trackClusterList;
		// Product genre list
		for (Cluster::pointer genre : genres)
		{
			if (!trackClusterList.empty())
				trackClusterList += ", ";
			trackClusterList += genre->getName();

			genre.modify()->addTrack(track);
		}

		track.modify()->setGenres( trackClusterList );
	}

	if ((*items).find(MetaData::Type::TrackNumber) != (*items).end())
		track.modify()->setTrackNumber( boost::any_cast<std::size_t>((*items)[MetaData::Type::TrackNumber]) );

	if ((*items).find(MetaData::Type::TotalTrack) != (*items).end())
		track.modify()->setTotalTrackNumber( boost::any_cast<std::size_t>((*items)[MetaData::Type::TotalTrack]) );

	if ((*items).find(MetaData::Type::DiscNumber) != (*items).end())
		track.modify()->setDiscNumber( boost::any_cast<std::size_t>((*items)[MetaData::Type::DiscNumber]) );

	if ((*items).find(MetaData::Type::TotalDisc) != (*items).end())
		track.modify()->setTotalDiscNumber( boost::any_cast<std::size_t>((*items)[MetaData::Type::TotalDisc]) );

	if ((*items).find(MetaData::Type::Date) != (*items).end())
		track.modify()->setDate( boost::any_cast<boost::posix_time::ptime>((*items)[MetaData::Type::Date]) );

	if ((*items).find(MetaData::Type::OriginalDate) != (*items).end())
	{
		track.modify()->setOriginalDate( boost::any_cast<boost::posix_time::ptime>((*items)[MetaData::Type::OriginalDate]) );

		// If a file has an OriginalDate but no date, set the date to ease filtering
		if ((*items).find(MetaData::Type::Date) == (*items).end())
			track.modify()->setDate( boost::any_cast<boost::posix_time::ptime>((*items)[MetaData::Type::OriginalDate]) );
	}

	if ((*items).find(MetaData::Type::MusicBrainzRecordingID) != (*items).end())
	{
		track.modify()->setMBID( boost::any_cast<std::string>((*items)[MetaData::Type::MusicBrainzRecordingID]) );
	}

	if ((*items).find(MetaData::Type::HasCover) != (*items).end())
	{
		bool hasCover = boost::any_cast<bool>((*items)[MetaData::Type::HasCover]);

		track.modify()->setCoverType( hasCover ? Track::CoverType::Embedded : Track::CoverType::None );
	}

	// TODO check added/modified
	_sigAddedTrack.emit(track);

	transaction.commit();
}

void
MediaScanner::processRootDirectory(boost::filesystem::path rootDirectory, Stats& stats)
{
	boost::system::error_code ec;

	boost::filesystem::recursive_directory_iterator itPath(rootDirectory, ec);

	boost::filesystem::recursive_directory_iterator itEnd;
	while (!ec && itPath != itEnd)
	{
		boost::filesystem::path path = *itPath;
		itPath.increment(ec);

		if (!_running)
			return;

		if (boost::filesystem::is_regular(path))
		{
			if (isFileSupported(path, _fileExtensions))
				processAudioFile(path, stats );
		}
	}
}

// Check if a file exists and is still in a root directory
static bool
checkFile(const boost::filesystem::path& p, const std::vector<boost::filesystem::path>& rootDirs, const std::vector<boost::filesystem::path>& extensions)
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
MediaScanner::checkAudioFiles( Stats& stats )
{
	LMS_LOG(DBUPDATER, INFO) << "Checking audio files...";

	std::vector<boost::filesystem::path> trackPaths = Track::getAllPaths(_db.getSession());;

	LMS_LOG(DBUPDATER, DEBUG) << "Checking tracks...";
	for (auto& trackPath : trackPaths)
	{
		if (!_running)
			return;

		if (!checkFile(trackPath, _rootDirectories, _fileExtensions))
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

	LMS_LOG(DBUPDATER, DEBUG) << "Checking Clusters...";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		// Now process orphan Cluster (no track)
		auto clusters = Cluster::getAll(_db.getSession());
		for (auto cluster : clusters)
		{
			if (cluster->getTracks().size() == 0)
			{
				LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan cluster '" << cluster->getName() << "'";
				cluster.remove();
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
MediaScanner::checkDuplicatedAudioFiles(Stats& stats)
{
	LMS_LOG(DBUPDATER, INFO) << "Checking duplicated audio files";

	Wt::Dbo::Transaction transaction(_db.getSession());

	std::vector<Track::pointer> tracks = Database::Track::getMBIDDuplicates(_db.getSession());
	for (Track::pointer track : tracks)
	{
		LMS_LOG(DBUPDATER, INFO) << "Found duplicated MBID [" << track->getMBID() << "], file: " << track->getPath() << " - " << track->getArtist()->getName() << " - " << track->getName();
	}

	tracks = Database::Track::getChecksumDuplicates(_db.getSession());
	for (Track::pointer track : tracks)
	{
		LMS_LOG(DBUPDATER, INFO) << "Found duplicated checksum [" << bufferToString(track->getChecksum()) << "], file: " << track->getPath() << " - " << track->getArtist()->getName() << " - " << track->getName();
	}


	LMS_LOG(DBUPDATER, INFO) << "Checking duplicated audio files done!";
}


} // namespace Scanner
