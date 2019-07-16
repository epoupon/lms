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
getOrCreateArtists(Session& session, const std::vector<MetaData::Artist>& artistsInfo)
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
getOrCreateRelease(Session& session, const MetaData::Album& album)
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
getOrCreateClusters(Session& session, const MetaData::Clusters& clustersNames)
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

MediaScanner::MediaScanner(std::unique_ptr<Database::Session> dbSession)
: _dbSession {std::move(dbSession)}
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

	scheduleNextScan();

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
MediaScanner::requestImmediateScan()
{
	_ioService.post([=]()
	{
		scheduleScan();
	});
}

void
MediaScanner::requestReschedule()
{
	_ioService.post([=]()
	{
		scheduleNextScan();
	});
}

MediaScanner::Status
MediaScanner::getStatus()
{
	Status res;

	std::unique_lock<std::mutex> lock {_statusMutex};
	res.currentState = _curState;
	res.nextScheduledScan = _nextScheduledScan;
	res.lastScanStats = _lastScanStats;
	res.inProgressStats = _inProgressStats;

	return res;
}

void
MediaScanner::scheduleNextScan()
{
	LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan";

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

	Wt::WDateTime nextScanDateTime;

	if (nextScanDate.isValid())
	{
		nextScanDateTime = Wt::WDateTime {nextScanDate, _startTime};
		scheduleScan(nextScanDateTime);
	}

	{
		std::unique_lock<std::mutex> lock {_statusMutex};
		_curState = nextScanDateTime.isValid() ? State::Scheduled : State::NotScheduled;
		_nextScheduledScan = nextScanDateTime;
	}

	_sigScheduled.emit(_nextScheduledScan);
}

void
MediaScanner::countAllFiles(Stats& stats)
{
	boost::system::error_code ec;

	stats.totalFiles = 0;

	boost::filesystem::recursive_directory_iterator itPath(_mediaDirectory, ec);
	if (ec)
	{
		LMS_LOG(DBUPDATER, ERROR) << "Cannot iterate over '" << _mediaDirectory.string() << "': " << ec.message();
		return;
	}

	boost::filesystem::recursive_directory_iterator itEnd;
	while (itPath != itEnd && _running)
	{
		if (stats.totalFiles % 250 == 0)
			notifyInProgressIfNeeded(stats);

		const boost::filesystem::path& path {*itPath++};

		if (!_running)
			break;

		if (boost::filesystem::is_regular(path)	&& isFileSupported(path, _fileExtensions))
			stats.totalFiles++;
	}
}

void
MediaScanner::scheduleScan(const Wt::WDateTime& dateTime)
{
	if (dateTime.isNull())
	{
		LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan right now";
		_scheduleTimer.expires_from_now(std::chrono::seconds(0));
		_scheduleTimer.async_wait(std::bind(&MediaScanner::scan, this, std::placeholders::_1));
	}
	else
	{
		std::chrono::system_clock::time_point timePoint {dateTime.toTimePoint()};
		std::time_t t {std::chrono::system_clock::to_time_t(timePoint)};

		LMS_LOG(DBUPDATER, INFO) << "Scheduling next scan at " << std::string(std::ctime(&t));
		_scheduleTimer.expires_at(timePoint);
		_scheduleTimer.async_wait(std::bind(&MediaScanner::scan, this, std::placeholders::_1));
	}
}

void
MediaScanner::scan(boost::system::error_code err)
{
	if (err)
		return;

	{
		std::unique_lock<std::mutex> lock {_statusMutex};
		_curState = State::InProgress;
		_nextScheduledScan = {};
		_inProgressStats = Stats{};
		_inProgressStats->startTime = Wt::WLocalDateTime::currentDateTime().toUTC();
	}

	Stats& stats {*_inProgressStats};

	LMS_LOG(UI, INFO) << "New scan started!";

	refreshScanSettings();

	bool forceScan {false};

	LMS_LOG(DBUPDATER, DEBUG) << "Counting files in media directory '" << _mediaDirectory.string() << "'...";
	countAllFiles(stats);
	LMS_LOG(DBUPDATER, DEBUG) << "-> Nb files = " << stats.totalFiles;

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
	}

	if (_running)
	{
		stats.stopTime = Wt::WLocalDateTime::currentDateTime().toUTC();
		{
			std::unique_lock<std::mutex> lock {_statusMutex};
			_lastScanStats = std::move(_inProgressStats);
			_inProgressStats.reset();
		}

		scheduleNextScan();

		scanComplete().emit(stats);
	}
	else
	{
		std::unique_lock<std::mutex> lock {_statusMutex};
		_curState = State::NotScheduled;
		_inProgressStats.reset();
	}

	LMS_LOG(DBUPDATER, INFO) << "Optimizing db...";
	_dbSession->optimize();
	LMS_LOG(DBUPDATER, INFO) << "Optimize db done!";
}

void
MediaScanner::refreshScanSettings()
{
	{
		auto transaction {_dbSession->createSharedTransaction()};

		ScanSettings::pointer scanSettings {ScanSettings::get(*_dbSession)};

		LMS_LOG(DBUPDATER, INFO) << "Using scan settings version " << scanSettings->getScanVersion();

		_scanVersion = scanSettings->getScanVersion();
		_startTime = scanSettings->getUpdateStartTime();
		_updatePeriod = scanSettings->getUpdatePeriod();

		_fileExtensions = scanSettings->getAudioFileExtensions();
		_mediaDirectory = scanSettings->getMediaDirectory();

		auto clusterTypes = scanSettings->getClusterTypes();
		std::set<std::string> clusterTypeNames;

		std::transform(std::cbegin(clusterTypes), std::cend(clusterTypes),
				std::inserter(clusterTypeNames, clusterTypeNames.begin()),
				[](ClusterType::pointer clusterType) { return clusterType->getName(); });

		_metadataParser.setClusterTypeNames(clusterTypeNames);
	}

	for (auto& addon : _addons)
		addon->refreshSettings();
}

void MediaScanner::notifyInProgress(Stats& stats)
{
	std::chrono::system_clock::time_point now {std::chrono::system_clock::now()};
	_sigScanInProgress(stats);
	_lastScanInProgressEmit = now;
}

void MediaScanner::notifyInProgressIfNeeded(Stats& stats)
{
	std::chrono::system_clock::time_point now {std::chrono::system_clock::now()};

	if (std::chrono::duration_cast<std::chrono::seconds>(now - _lastScanInProgressEmit).count() > 2)
	{
		_sigScanInProgress(stats);
		_lastScanInProgressEmit = now;
	}
}

void
MediaScanner::scanAudioFile(const boost::filesystem::path& file, bool forceScan, Stats& stats)
{
	notifyInProgressIfNeeded(stats);

	auto lastWriteTime = Wt::WDateTime::fromTime_t(boost::filesystem::last_write_time(file));

	if (!forceScan)
	{
		// Skip file if last write is the same
		auto transaction {_dbSession->createSharedTransaction()};

		Track::pointer track {Track::getByPath(*_dbSession, file)};

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

	auto uniqueTransaction {_dbSession->createUniqueTransaction()};

	Track::pointer track {Track::getByPath(*_dbSession, file) };

	// We estimate this is an audio file if:
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
	std::vector<Cluster::pointer> clusters {getOrCreateClusters(*_dbSession, trackInfo->clusters)};

	//  ***** Artists
	std::vector<Artist::pointer> artists {getOrCreateArtists(*_dbSession, trackInfo->artists)};

	//  ***** Release artists
	std::vector<Artist::pointer> releaseArtists {getOrCreateArtists(*_dbSession, trackInfo->albumArtists)};

	//  ***** Release
	Release::pointer release;
	if (trackInfo->album)
		release = getOrCreateRelease(*_dbSession, *trackInfo->album);

	// If file already exist, update data
	// Otherwise, create it
	if (!track)
	{
		// Create a new song
		track = Track::create(*_dbSession, file);
		LMS_LOG(DBUPDATER, INFO) << "Adding '" << file.string() << "'";
		stats.additions++;
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
		track.modify()->addArtistLink(Database::TrackArtistLink::create(*_dbSession, track, artist, Database::TrackArtistLink::Type::Artist));

	for (const auto& releaseArtist : releaseArtists)
		track.modify()->addArtistLink(Database::TrackArtistLink::create(*_dbSession, track, releaseArtist, Database::TrackArtistLink::Type::ReleaseArtist));

	track.modify()->setScanVersion(_scanVersion);
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
		const boost::filesystem::path& path {*itPath};

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

	notifyInProgress(stats);
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
	std::vector<boost::filesystem::path> trackPaths;
	{
		auto transaction {_dbSession->createSharedTransaction()};
		trackPaths = Track::getAllPaths(*_dbSession);;
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking tracks...";
	for (const auto& trackPath : trackPaths)
	{
		if (!_running)
			return;

		if (!checkFile(trackPath, _mediaDirectory, _fileExtensions))
		{
			auto transaction {_dbSession->createUniqueTransaction()};

			Track::pointer track {Track::getByPath(*_dbSession, trackPath)};
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
		auto transaction {_dbSession->createUniqueTransaction()};

		// Now process orphan Cluster (no track)
		auto clusters {Cluster::getAllOrphans(*_dbSession)};
		for (auto& cluster : clusters)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan cluster '" << cluster->getName() << "'";
			cluster.remove();
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan artists...";
	{
		auto transaction {_dbSession->createUniqueTransaction()};

		auto artists {Artist::getAllOrphans(*_dbSession)};
		for (auto& artist : artists)
		{
			LMS_LOG(DBUPDATER, DEBUG) << "Removing orphan artist '" << artist->getName() << "'";
			artist.remove();
		}
	}

	LMS_LOG(DBUPDATER, DEBUG) << "Checking orphan releases...";
	{
		auto transaction {_dbSession->createUniqueTransaction()};

		auto releases {Release::getAllOrphans(*_dbSession)};
		for (auto& release : releases)
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

	auto transaction {_dbSession->createSharedTransaction()};

	const std::vector<Track::pointer> tracks = Database::Track::getMBIDDuplicates(*_dbSession);
	for (const Track::pointer& track : tracks)
	{
		LMS_LOG(DBUPDATER, INFO) << "Found duplicated MBID [" << track->getMBID() << "], file: " << track->getPath().string() << " - " << track->getName();
		stats.duplicateMBID++;
	}

	LMS_LOG(DBUPDATER, INFO) << "Checking duplicated audio files done!";
}

} // namespace Scanner
