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

#include "MediaScanner.hpp"

#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/asio/placeholders.hpp>

#include <Wt/WLocalDateTime.h>

#include "cover/CoverArtGrabber.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/Track.hpp"
#include "utils/Logger.hpp"
#include "utils/Path.hpp"
#include "utils/Utils.hpp"

using namespace Database;

namespace {

Wt::WDate
getNextMonday(Wt::WDate current)
{
	do
	{
		current = current.addDays(1);
	} while (current.dayOfWeek() != 1);

	return current;
}

Wt::WDate
getNextFirstOfMonth(Wt::WDate current)
{
	do
	{
		current = current.addDays(1);
	} while (current.day() != 1);

	return current;
}

bool
isFileSupported(const boost::filesystem::path& file, const std::set<boost::filesystem::path>& extensions)
{
	return (extensions.find(file.extension()) != extensions.end());
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

std::vector<Artist::pointer>
getOrCreateArtists(Wt::Dbo::Session& session, const std::vector<MetaData::Artist>& artistsInfo)
{
	std::vector<Artist::pointer> artists;

	for (const MetaData::Artist& artistInfo : artistsInfo)
	{
		Artist::pointer artist;

		// First try to get by MBID
		if (!artistInfo.musicBrainzArtistID.empty())
		{
			artist = Artist::getByMBID(session, artistInfo.musicBrainzArtistID);
			if (!artist)
				artist = Artist::create(session, artistInfo.name, artistInfo.musicBrainzArtistID);

			artists.emplace_back(std::move(artist));
			continue;
		}

		// Fall back on artist name (collisions may occur)
		if (!artistInfo.name.empty())
		{
			for (const Artist::pointer& sameNamedArtist : Artist::getByName(session, artistInfo.name))
			{
				if (sameNamedArtist->getMBID().empty())
				{
					artist = sameNamedArtist;
					break;
				}
			}

			// No Artist found with the same name and without MBID -> creating
			if (!artist)
				artist = Artist::create(session, artistInfo.name);

			artists.emplace_back(std::move(artist));
			continue;
		}
	}

	return artists;
}

Release::pointer
getOrCreateRelease(Wt::Dbo::Session& session, const MetaData::Album& album)
{
	Release::pointer release;

	// First try to get by MBID
	if (!album.musicBrainzAlbumID.empty())
	{
		release = Release::getByMBID(session, album.musicBrainzAlbumID);
		if (!release)
			release = Release::create(session, album.name, album.musicBrainzAlbumID);

		return release;
	}

	// Fall back on release name (collisions may occur)
	if (!album.name.empty())
	{
		for (const Release::pointer& sameNamedRelease : Release::getByName(session, album.name))
		{
			if (sameNamedRelease->getMBID().empty())
			{
				release = sameNamedRelease;
				break;
			}
		}

		// No release found with the same name and without MBID -> creating
		if (!release)
			release = Release::create(session, album.name);

		return release;
	}

	return Release::pointer{};
}

std::vector<Cluster::pointer>
getOrCreateClusters(Wt::Dbo::Session& session, const MetaData::Clusters& clustersNames)
{
	std::vector< Cluster::pointer > clusters;

	for (auto clusterNames : clustersNames)
	{
		auto clusterType = ClusterType::getByName(session, clusterNames.first);
		if (!clusterType)
			continue;

		for (auto clusterName : clusterNames.second)
		{
			auto cluster = clusterType->getCluster(clusterName);
			if (!cluster)
				cluster = Cluster::create(session, clusterType, clusterName);

			clusters.push_back(cluster);
		}
	}

	return clusters;
}

} // namespace

namespace Scanner {

MediaScanner::MediaScanner(Wt::Dbo::SqlConnectionPool& connectionPool)
 : _running(false),
_scheduleTimer(_ioService),
_db(connectionPool)
{
	_ioService.setThreadCount(1);

	refreshScanSettings();
}

void
MediaScanner::setAddon(MediaScannerAddon& addon)
{
	_addons.push_back(&addon);
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
	scheduleScan();

	_ioService.start();
}

void
MediaScanner::stop(void)
{
	_running = false;

	for (auto& addon : _addons)
		addon->requestStop();

	_scheduleTimer.cancel();

	_ioService.stop();
}

void
MediaScanner::scheduleImmediateScan()
{
	_ioService.post([=]()
	{
		LMS_LOG(DBUPDATER, INFO) << "Schedule immediate scan";
		scheduleScan(std::chrono::seconds(0));
	});
}

void
MediaScanner::reschedule()
{
	_ioService.post([=]()
	{
		LMS_LOG(DBUPDATER, INFO) << "Rescheduling scan";
		scheduleScan();
	});
}

void
MediaScanner::scheduleScan()
{
	refreshScanSettings();

	Wt::WDateTime now = Wt::WLocalDateTime::currentServerDateTime().toUTC();

	Wt::WDate nextScanDate;
	switch (_updatePeriod)
	{
		case ScanSettings::UpdatePeriod::Daily:
			if (now.time() < _startTime)
				nextScanDate = now.date();
			else
				nextScanDate = now.date().addDays(1);
			break;

		case ScanSettings::UpdatePeriod::Weekly:
			if (now.time() < _startTime && now.date().dayOfWeek() == 1)
				nextScanDate = now.date();
			else
				nextScanDate = getNextMonday(now.date());
			break;

		case ScanSettings::UpdatePeriod::Monthly:
			if (now.time() < _startTime && now.date().day() == 1)
				nextScanDate = now.date();
			else
				nextScanDate = getNextFirstOfMonth(now.date());
			break;

		case ScanSettings::UpdatePeriod::Never:
			LMS_LOG(DBUPDATER, INFO) << "Auto scan disabled!";
			break;
	}

	if (nextScanDate.isValid())
		scheduleScan( Wt::WDateTime(nextScanDate, _startTime).toTimePoint() );
}

void
MediaScanner::scheduleScan(std::chrono::seconds duration)
{
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan in " << duration.count() << " seconds";
	_scheduleTimer.expires_from_now(duration);
	_scheduleTimer.async_wait( std::bind( &MediaScanner::scan, this, std::placeholders::_1) );
}

void
MediaScanner::scheduleScan(std::chrono::system_clock::time_point timePoint)
{
	std::time_t t = std::chrono::system_clock::to_time_t(timePoint);
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan at " << std::string(std::ctime(&t));
	_scheduleTimer.expires_at(timePoint);
	_scheduleTimer.async_wait(std::bind(&MediaScanner::scan, this, std::placeholders::_1));
}

void
MediaScanner::scan(boost::system::error_code err)
{
	if (err)
		return;

	LMS_LOG(UI, INFO) << "New scan started!";

	refreshScanSettings();

	bool forceScan = false;
	Stats stats;

	removeMissingTracks(stats);

	LMS_LOG(UI, INFO) << "Checks complete, force scan = " << forceScan;

	LMS_LOG(DBUPDATER, INFO) << "scaning media directory '" << _mediaDirectory.string() << "'...";
	scanMediaDirectory(_mediaDirectory, forceScan, stats);
	LMS_LOG(DBUPDATER, INFO) << "scaning media directory '" << _mediaDirectory.string() << "' DONE";

	if (_running)
	{
		removeOrphanEntries();
		checkDuplicatedAudioFiles(stats);
	}

	LMS_LOG(DBUPDATER, INFO) << "Scan " << (_running ? "complete" : "aborted") << ". Changes = " << stats.nbChanges() << " (added = " << stats.additions << ", removed = " << stats.deletions << ", updated = " << stats.updates << "), Not changed = " << stats.skips << ", Scanned = " << stats.scans << " (errors = " << stats.scanErrors << ", not imported = " << stats.incompleteScans << "), duplicates = " << stats.nbDuplicates() << " (hash = " << stats.duplicateHashes << ", mbid = " << stats.duplicateMBID << ")";

	if (_running)
	{
		for (auto& addon : _addons)
			addon->preScanComplete();

		scheduleScan();

		scanComplete().emit(stats);
	}
}

void
MediaScanner::refreshScanSettings()
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	auto scanSettings = ScanSettings::get(_db.getSession());

	LMS_LOG(DBUPDATER, INFO) << "Using scan settings version " << scanSettings->getScanVersion();

	_scanVersion = scanSettings->getScanVersion();
	_startTime = scanSettings->getUpdateStartTime();
	_updatePeriod = scanSettings->getUpdatePeriod();

	_fileExtensions = scanSettings->getAudioFileExtensions();
	_mediaDirectory = scanSettings->getMediaDirectory();

	auto clusterTypes = scanSettings->getClusterTypes();
	std::set<std::string> clusterTypeNames;

	std::transform(clusterTypes.begin(), clusterTypes.end(),
			std::inserter(clusterTypeNames, clusterTypeNames.begin()),
			[](ClusterType::pointer clusterType) -> std::string { return clusterType->getName(); });

	_metadataParser.setClusterTypeNames(clusterTypeNames);

	transaction.commit();

	for (auto& addon : _addons)
		addon->refreshSettings();
}

void
MediaScanner::scanAudioFile(const boost::filesystem::path& file, bool forceScan, Stats& stats)
{
	auto lastWriteTime = Wt::WDateTime::fromTime_t(boost::filesystem::last_write_time(file));

	if (!forceScan)
	{
		// Skip file if last write is the same
		Wt::Dbo::Transaction transaction(_db.getSession());

		Wt::Dbo::ptr<Track> track = Track::getByPath(_db.getSession(), file);

		if (track && track->getLastWriteTime() == lastWriteTime && track->getScanVersion() == _scanVersion)
		{
			stats.skips++;
			return;
		}
	}

	boost::optional<MetaData::Track> trackInfo {_metadataParser.parse(file)};
	if (!trackInfo)
	{
		stats.scanErrors++;
		return;
	}

	stats.scans++;

	std::vector<unsigned char> checksum ;
	computeCrc(file, checksum);

	Wt::Dbo::Transaction transaction {_db.getSession()};

	Wt::Dbo::ptr<Track> track {Track::getByPath(_db.getSession(), file) };

	// We estimate this is a audio file if:
	// - we found a least one audio stream
	// - the duration is not null
	if (trackInfo->audioStreams.empty())
	{
		LMS_LOG(DBUPDATER, INFO) << "Skipped '" << file.string() << "' (no audio stream found)";

		// If Track exists here, delete it!
		if (track)
		{
			track.remove();
			stats.deletions++;
		}
		stats.incompleteScans++;
		return;
	}
	if (trackInfo->duration == std::chrono::milliseconds::zero())
	{
		LMS_LOG(DBUPDATER, INFO) << "Skipped '" << file.string() << "' (duration is 0)";

		// If Track exists here, delete it!
		if (track)
		{
			track.remove();
			stats.deletions++;
		}
		stats.incompleteScans++;
		return;
	}

	// ***** Title
	std::string title;
	if (!trackInfo->title.empty())
		title = trackInfo->title;
	else
	{
		// TODO parse file name guess track etc.
		// For now juste use file name as title
		title = file.filename().string();
	}

	// ***** Clusters
	std::vector<Cluster::pointer> clusters {getOrCreateClusters(_db.getSession(), trackInfo->clusters)};

	//  ***** Artists
	std::vector<Artist::pointer> artists {getOrCreateArtists(_db.getSession(), trackInfo->artists)};

	//  ***** Release artists
	std::vector<Artist::pointer> releaseArtists {getOrCreateArtists(_db.getSession(), trackInfo->albumArtists)};

	//  ***** Release
	Release::pointer release;
	if (trackInfo->album)
		release = getOrCreateRelease(_db.getSession(), *trackInfo->album);

	// If file already exist, update data
	// Otherwise, create it
	bool trackAdded {false};
	if (!track)
	{
		// Create a new song
		track = Track::create(_db.getSession(), file);
		LMS_LOG(DBUPDATER, INFO) << "Adding '" << file.string() << "'";
		stats.additions++;
		trackAdded = true;
	}
	else
	{
		LMS_LOG(DBUPDATER, INFO) << "Updating '" << file.string() << "'";

		stats.updates++;
	}

	// Release related data
	if (release)
	{
		release.modify()->setTotalTrackNumber(trackInfo->totalTrack ? *trackInfo->totalTrack : 0);
		release.modify()->setTotalDiscNumber(trackInfo->totalDisc ? *trackInfo->totalDisc : 0);
	}

	// Track related data
	assert(track);

	track.modify()->clearArtistLinks();
	for (const auto& artist : artists)
		track.modify()->addArtistLink(Database::TrackArtistLink::create(_db.getSession(), track, artist, Database::TrackArtistLink::Type::Artist));

	for (const auto& releaseArtist : releaseArtists)
		track.modify()->addArtistLink(Database::TrackArtistLink::create(_db.getSession(), track, releaseArtist, Database::TrackArtistLink::Type::ReleaseArtist));

	track.modify()->setScanVersion(_scanVersion);
	track.modify()->setChecksum(checksum);
	track.modify()->setRelease(release);
	track.modify()->setClusters(clusters);
	track.modify()->setLastWriteTime(lastWriteTime);
	track.modify()->setName(title);
	track.modify()->setDuration(trackInfo->duration);
	track.modify()->setAddedTime(Wt::WLocalDateTime::currentServerDateTime().toUTC());
	track.modify()->setTrackNumber(trackInfo->trackNumber ? *trackInfo->trackNumber : 0);
	track.modify()->setDiscNumber(trackInfo->discNumber ? *trackInfo->discNumber : 0);
	track.modify()->setYear(trackInfo->year ? *trackInfo->year : 0);
	track.modify()->setOriginalYear(trackInfo->originalYear ? *trackInfo->originalYear : 0);

	// If a file has an OriginalYear but no Year, set it to ease filtering
	if (!trackInfo->year && trackInfo->originalYear)
		track.modify()->setYear(*trackInfo->originalYear);

	track.modify()->setMBID(trackInfo->musicBrainzRecordID);
	track.modify()->setHasCover(trackInfo->hasCover);
	track.modify()->setCopyright(trackInfo->copyright);
	track.modify()->setCopyrightURL(trackInfo->copyrightURL);

	transaction.commit();

	for (auto& addon : _addons)
	{
		if (trackAdded)
			addon->trackAdded(track.id());
		else
			addon->trackUpdated(track.id());
	}
}

void
MediaScanner::scanMediaDirectory(boost::filesystem::path mediaDirectory, bool forceScan, Stats& stats)
{
	boost::system::error_code ec;

	boost::filesystem::recursive_directory_iterator itPath(mediaDirectory, ec);
	if (ec)
	{
		LMS_LOG(DBUPDATER, ERROR) << "Cannot iterate over '" << mediaDirectory.string() << "': " << ec.message();
		return;
	}

	boost::filesystem::recursive_directory_iterator itEnd;
	while (itPath != itEnd)
	{
		boost::filesystem::path path = *itPath;

		if (!_running)
			return;

		if (ec)
		{
			LMS_LOG(DBUPDATER, ERROR) << "Cannot process entry: " << ec.message();
		}
		else if (boost::filesystem::is_regular(path))
		{
			if (isFileSupported(path, _fileExtensions))
				scanAudioFile(path, forceScan, stats );
		}

		itPath.increment(ec);
	}
}

// Check if a file exists and is still in a media directory
static bool
checkFile(const boost::filesystem::path& p, const boost::filesystem::path& mediaDirectory, const std::set<boost::filesystem::path>& extensions)
{
	try
	{
		// For each track, make sure the the file still exists
		// and still belongs to a media directory
		if (!boost::filesystem::exists( p )
			|| !boost::filesystem::is_regular( p ) )
		{
			LMS_LOG(DBUPDATER, INFO) << "Removing '" << p.string() << "': missing";
			return false;
		}

		if (!isPathInParentPath(p, mediaDirectory))
		{
			LMS_LOG(DBUPDATER, INFO) << "Removing '" << p.string() << "': out of media directory";
			return false;
		}

		if (!isFileSupported(p, extensions))
		{
			LMS_LOG(DBUPDATER, INFO) << "Removing '" << p.string() << "': file format no longer handled";
			return false;
		}

		return true;

	}
	catch (boost::filesystem::filesystem_error& e)
	{
		LMS_LOG(DBUPDATER, ERROR) << "Caught exception while checking file '" << p.string() << "': " << e.what();
		return false;
	}
}

void
MediaScanner::removeMissingTracks(Stats& stats)
{
	std::vector<boost::filesystem::path> trackPaths = Track::getAllPaths(_db.getSession());;

	LMS_LOG(DBUPDATER, DEBUG) << "Checking tracks...";
	for (const auto& trackPath : trackPaths)
	{
		if (!_running)
			return;

		if (!checkFile(trackPath, _mediaDirectory, _fileExtensions))
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Track::pointer track = Track::getByPath(_db.getSession(), trackPath);
			if (track)
			{
				track.remove();
				stats.deletions++;
			}
		}
	}
}

void
MediaScanner::removeOrphanEntries()
{
	LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan clusters...";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		// Now process orphan Cluster (no track)
		auto clusters = Cluster::getAllOrphans(_db.getSession());
		for (auto cluster : clusters)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan cluster '" << cluster->getName() << "'";
			cluster.remove();
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan artists...";
	{
		Wt::Dbo::Transaction transaction(_db.getSession());

		auto artists = Artist::getAllOrphans(_db.getSession());
		for (auto artist : artists)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan artist '" << artist->getName() << "'";
			artist.remove();
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan releases...";
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
		LMS_LOG(DBUPDATER, INFO) << "Found duplicated MBID [" << track->getMBID() << "], file: " << track->getPath().string() << " - " << track->getName();
		stats.duplicateMBID++;
	}

	tracks = Database::Track::getChecksumDuplicates(_db.getSession());
	for (Track::pointer track : tracks)
	{
		LMS_LOG(DBUPDATER, INFO) << "Found duplicated checksum [" << bufferToString(track->getChecksum()) << "], file: " << track->getPath().string() << " - " << track->getName();
		stats.duplicateHashes++;
	}


	LMS_LOG(DBUPDATER, INFO) << "Checking duplicated audio files done!";
}

} // namespace Scanner
