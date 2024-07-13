/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ScanStepScanFiles.hpp"

#include "core/Exception.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Path.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/TrackFeatures.hpp"
#include "metadata/Exception.hpp"
#include "metadata/IParser.hpp"

namespace lms::scanner
{
    using namespace db;

    namespace
    {
        struct FileInfo
        {
            Wt::WDateTime lastWriteTime;
            std::filesystem::path relativePath;
            std::size_t fileSize{};
        };

        Wt::WDateTime retrieveFileGetLastWrite(const std::filesystem::path& file)
        {
            Wt::WDateTime res;

            try
            {
                res = core::pathUtils::getLastWriteTime(file);
            }
            catch (core::LmsException& e)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot get last write time: " << e.what());
            }

            return res;
        }

        std::optional<FileInfo> retrieveFileInfo(const std::filesystem::path& file, const std::filesystem::path& rootPath)
        {
            std::optional<FileInfo> res;
            res.emplace();

            res->lastWriteTime = retrieveFileGetLastWrite(file);
            if (!res->lastWriteTime.isValid())
            {
                res.reset();
                return res;
            }

            {
                std::error_code ec;
                res->relativePath = std::filesystem::relative(file, rootPath, ec);
                if (ec)
                {
                    LMS_LOG(DBUPDATER, ERROR, "Cannot get relative file path for '" << file.string() << "' from '" << rootPath.string() << "': " << ec.message());
                    res.reset();
                    return res;
                }
            }

            {
                std::error_code ec;
                res->fileSize = std::filesystem::file_size(file, ec);
                if (ec)
                {
                    LMS_LOG(DBUPDATER, ERROR, "Cannot get file size for '" << file.string() << "': " << ec.message());
                    res.reset();
                    return res;
                }
            }

            return res;
        }

        Directory::pointer getOrCreateDirectory(Session& session, const std::filesystem::path& path, const MediaLibrary::pointer& mediaLibrary)
        {
            Directory::pointer directory{ Directory::find(session, path) };
            if (!directory)
            {
                Directory::pointer parentDirectory;
                if (path != mediaLibrary->getPath())
                    parentDirectory = getOrCreateDirectory(session, path.parent_path(), mediaLibrary);

                directory = session.create<Directory>(path);
                directory.modify()->setParent(parentDirectory);
                directory.modify()->setMediaLibrary(mediaLibrary);
            }
            // Don't update library if it does not match, will be updated elsewhere

            return directory;
        }

        Artist::pointer createArtist(Session& session, const metadata::Artist& artistInfo)
        {
            Artist::pointer artist{ session.create<Artist>(artistInfo.name) };

            if (artistInfo.mbid)
                artist.modify()->setMBID(*artistInfo.mbid);
            if (artistInfo.sortName)
                artist.modify()->setSortName(*artistInfo.sortName);

            return artist;
        }

        void updateArtistIfNeeded(Artist::pointer artist, const metadata::Artist& artistInfo)
        {
            // Name may have been updated
            if (artist->getName() != artistInfo.name)
            {
                artist.modify()->setName(artistInfo.name);
            }

            // Sortname may have been updated
            if (artistInfo.sortName && *artistInfo.sortName != artist->getSortName())
            {
                artist.modify()->setSortName(*artistInfo.sortName);
            }
        }

        std::vector<Artist::pointer> getOrCreateArtists(Session& session, const std::vector<metadata::Artist>& artistsInfo, bool allowFallbackOnMBIDEntries)
        {
            std::vector<Artist::pointer> artists;

            for (const metadata::Artist& artistInfo : artistsInfo)
            {
                Artist::pointer artist;

                // First try to get by MBID
                if (artistInfo.mbid)
                {
                    artist = Artist::find(session, *artistInfo.mbid);
                    if (!artist)
                        artist = createArtist(session, artistInfo);
                    else
                        updateArtistIfNeeded(artist, artistInfo);

                    artists.emplace_back(std::move(artist));
                    continue;
                }

                // Fall back on artist name (collisions may occur)
                if (!artistInfo.name.empty())
                {
                    for (const Artist::pointer& sameNamedArtist : Artist::find(session, artistInfo.name))
                    {
                        // Do not fallback on artist that is correctly tagged
                        if (!allowFallbackOnMBIDEntries && sameNamedArtist->getMBID())
                            continue;

                        artist = sameNamedArtist;
                        break;
                    }

                    // No Artist found with the same name and without MBID -> creating
                    if (!artist)
                        artist = createArtist(session, artistInfo);
                    else
                        updateArtistIfNeeded(artist, artistInfo);

                    artists.emplace_back(std::move(artist));
                    continue;
                }
            }

            return artists;
        }

        ReleaseType::pointer getOrCreateReleaseType(Session& session, std::string_view name)
        {
            ReleaseType::pointer releaseType{ ReleaseType::find(session, name) };
            if (!releaseType)
                releaseType = session.create<ReleaseType>(name);

            return releaseType;
        }

        void updateReleaseIfNeeded(Session& session, Release::pointer release, const metadata::Release& releaseInfo)
        {
            if (release->getName() != releaseInfo.name)
                release.modify()->setName(releaseInfo.name);
            if (release->getSortName() != releaseInfo.sortName)
                release.modify()->setSortName(releaseInfo.sortName);
            if (release->getGroupMBID() != releaseInfo.groupMBID)
                release.modify()->setGroupMBID(releaseInfo.groupMBID);
            if (release->getTotalDisc() != releaseInfo.mediumCount)
                release.modify()->setTotalDisc(releaseInfo.mediumCount);
            if (release->getArtistDisplayName() != releaseInfo.artistDisplayName)
                release.modify()->setArtistDisplayName(releaseInfo.artistDisplayName);
            if (release->getReleaseTypeNames() != releaseInfo.releaseTypes)
            {
                release.modify()->clearReleaseTypes();
                for (std::string_view releaseType : releaseInfo.releaseTypes)
                    release.modify()->addReleaseType(getOrCreateReleaseType(session, releaseType));
            }
        }

        Release::pointer getOrCreateRelease(Session& session, const metadata::Release& releaseInfo, const std::filesystem::path& expectedReleaseDirectory)
        {
            Release::pointer release;

            // First try to get by MBID
            if (releaseInfo.mbid)
            {
                release = Release::find(session, *releaseInfo.mbid);
                if (!release)
                    release = session.create<Release>(releaseInfo.name, releaseInfo.mbid);

                updateReleaseIfNeeded(session, release, releaseInfo);
                return release;
            }

            // Fall back on release name (collisions may occur), if and only if it is in the current directory
            if (!releaseInfo.name.empty())
            {
                for (const Release::pointer& sameNamedRelease : Release::find(session, releaseInfo.name, expectedReleaseDirectory))
                {
                    // do not fallback on properly tagged releases
                    if (sameNamedRelease->getMBID())
                        continue;

                    release = sameNamedRelease;
                    break;
                }

                // No release found with the same name and without MBID -> creating
                if (!release)
                    release = session.create<Release>(releaseInfo.name);

                updateReleaseIfNeeded(session, release, releaseInfo);
                return release;
            }

            return Release::pointer{};
        }

        std::vector<Cluster::pointer> getOrCreateClusters(Session& session, const metadata::Track& track)
        {
            std::vector<Cluster::pointer> clusters;

            auto getOrCreateClusters{ [&](std::string tag, std::span<const std::string> values) {
                auto clusterType = ClusterType::find(session, tag);
                if (!clusterType)
                    clusterType = session.create<ClusterType>(tag);

                for (const auto& value : values)
                {
                    auto cluster{ clusterType->getCluster(value) };
                    if (!cluster)
                        cluster = session.create<Cluster>(clusterType, value);

                    clusters.push_back(cluster);
                }
            } };

            // TODO: migrate these fields in dedicated tables in DB
            getOrCreateClusters("GENRE", track.genres);
            getOrCreateClusters("MOOD", track.moods);
            getOrCreateClusters("LANGUAGE", track.languages);
            getOrCreateClusters("GROUPING", track.groupings);

            for (const auto& [tag, values] : track.userExtraTags)
                getOrCreateClusters(tag, values);

            return clusters;
        }

        metadata::ParserReadStyle getParserReadStyle()
        {
            std::string_view readStyle{ core::Service<core::IConfig>::get()->getString("scanner-parser-read-style", "average") };

            if (readStyle == "fast")
                return metadata::ParserReadStyle::Fast;
            else if (readStyle == "average")
                return metadata::ParserReadStyle::Average;
            else if (readStyle == "accurate")
                return metadata::ParserReadStyle::Accurate;

            throw core::LmsException{ "Invalid value for 'scanner-parser-read-style'" };
        }

        std::size_t getScanMetaDataThreadCount()
        {
            std::size_t threadCount{ core::Service<core::IConfig>::get()->getULong("scanner-metadata-thread-count", 0) };

            if (threadCount == 0)
                threadCount = std::max<std::size_t>(std::thread::hardware_concurrency() / 2, 1);

            return threadCount;
        }
    } // namespace

    ScanStepScanFiles::ScanStepScanFiles(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _metadataParser{ metadata::createParser(metadata::ParserBackend::TagLib, getParserReadStyle()) } // For now, always use TagLib
        , _fileScanQueue{ *_metadataParser, getScanMetaDataThreadCount(), _abortScan }
    {
        LMS_LOG(DBUPDATER, INFO, "Using " << _fileScanQueue.getThreadCount() << " thread(s) for scanning file metadata");
    }

    void ScanStepScanFiles::process(ScanContext& context)
    {
        {
            std::vector<std::string> tagsToParse{ _extraTagsToParse };
            tagsToParse.insert(std::end(tagsToParse), std::cbegin(_settings.extraTags), std::cend(_settings.extraTags));
            _metadataParser->setUserExtraTags(tagsToParse);
            _metadataParser->setArtistTagDelimiters(_settings.artistTagDelimiters);
            _metadataParser->setDefaultTagDelimiters(_settings.defaultTagDelimiters);
        }

        context.currentStepStats.totalElems = context.stats.totalFileCount;

        for (const ScannerSettings::MediaLibraryInfo& mediaLibrary : _settings.mediaLibraries)
            process(context, mediaLibrary);
    }

    void ScanStepScanFiles::process(ScanContext& context, const ScannerSettings::MediaLibraryInfo& mediaLibrary)
    {
        const std::size_t scanQueueMaxScanRequestCount{ 100 * _fileScanQueue.getThreadCount() };
        const std::size_t processFileResultsBatchSize{ 5 };

        std::vector<FileScanResult> scanResults;

        std::filesystem::path currentDirectory;
        core::pathUtils::exploreFilesRecursive(
            mediaLibrary.rootDirectory, [&](std::error_code ec, const std::filesystem::path& path) {
                LMS_SCOPED_TRACE_DETAILED("Scanner", "OnExploreFile");

                if (_abortScan)
                    return false; // stop iterating

                if (ec)
                {
                    LMS_LOG(DBUPDATER, ERROR, "Cannot scan file '" << path.string() << "': " << ec.message());
                    context.stats.errors.emplace_back(ScanError{ path, ScanErrorType::CannotReadFile, ec.message() });
                }
                else
                {
                    bool fileMatched{};
                    if (core::pathUtils::hasFileAnyExtension(path, _settings.supportedAudioFileExtensions))
                    {
                        fileMatched = true;
                        if (checkAudioFileNeedScan(context, path, mediaLibrary))
                            _fileScanQueue.pushScanRequest(path, FileScanQueue::ScanRequestType::AudioFile);
                    }
                    else if (core::pathUtils::hasFileAnyExtension(path, _settings.supportedImageFileExtensions))
                    {
                        fileMatched = true;
                        if (checkImageFileNeedScan(context, path))
                            _fileScanQueue.pushScanRequest(path, FileScanQueue::ScanRequestType::ImageFile);
                    }

                    if (fileMatched)
                    {
                        // Not very efficient way to update media_library for directories
                        if (path.has_parent_path())
                        {
                            const std::filesystem::path directory{ path.parent_path() };
                            if (directory != currentDirectory)
                            {
                                updateDirectoryIfNeeded(currentDirectory, mediaLibrary);
                                currentDirectory = directory;
                            }
                        }

                        context.currentStepStats.processedElems++;
                        _progressCallback(context.currentStepStats);
                    }
                }

                while (_fileScanQueue.getResultsCount() > (scanQueueMaxScanRequestCount / 2))
                {
                    _fileScanQueue.popResults(scanResults, processFileResultsBatchSize);
                    processFileScanResults(context, scanResults, mediaLibrary);
                }

                _fileScanQueue.wait(scanQueueMaxScanRequestCount);

                return true;
            },
            &excludeDirFileName);

        _fileScanQueue.wait();

        while (!_abortScan && _fileScanQueue.popResults(scanResults, processFileResultsBatchSize) > 0)
            processFileScanResults(context, scanResults, mediaLibrary);
    }

    bool ScanStepScanFiles::checkAudioFileNeedScan(ScanContext& context, const std::filesystem::path& file, const ScannerSettings::MediaLibraryInfo& libraryInfo)
    {
        ScanStats& stats{ context.stats };

        const Wt::WDateTime lastWriteTime{ retrieveFileGetLastWrite(file) };
        // Should rarely fail as we are currently iterating it
        if (!lastWriteTime.isValid())
        {
            stats.skips++;
            return false;
        }

        if (context.scanOptions.fullScan)
            return true;

        bool needUpdateLibrary{};
        db::Session& dbSession{ _db.getTLSSession() };

        {
            auto transaction{ dbSession.createReadTransaction() };

            // Skip file if last write is the same
            const Track::pointer track{ Track::findByPath(dbSession, file) };
            if (track
                && track->getLastWriteTime() == lastWriteTime
                && track->getScanVersion() == _settings.scanVersion)
            {
                // this file may have been moved from one library to another, then we just need to update the media library id instead of a full rescan
                const auto trackMediaLibrary{ track->getMediaLibrary() };
                if (trackMediaLibrary && trackMediaLibrary->getId() == libraryInfo.id)
                {
                    stats.skips++;
                    return false;
                }

                needUpdateLibrary = true;
            }
        }

        if (needUpdateLibrary)
        {
            auto transaction{ dbSession.createWriteTransaction() };

            Track::pointer track{ Track::findByPath(dbSession, file) };
            assert(track);
            track.modify()->setMediaLibrary(db::MediaLibrary::find(dbSession, libraryInfo.id)); // may be null, will be handled in the next scan anyway
            stats.updates++;
            return false;
        }

        return true; // need to scan
    }

    bool ScanStepScanFiles::checkImageFileNeedScan(ScanContext& context, const std::filesystem::path& file)
    {
        ScanStats& stats{ context.stats };

        const Wt::WDateTime lastWriteTime{ retrieveFileGetLastWrite(file) };
        // Should rarely fail as we are currently iterating it
        if (!lastWriteTime.isValid())
        {
            stats.skips++;
            return false;
        }

        if (!context.scanOptions.fullScan)
        {
            db::Session& dbSession{ _db.getTLSSession() };
            auto transaction{ _db.getTLSSession().createReadTransaction() };

            const db::Image::pointer image{ db::Image::find(dbSession, file) };
            if (image && image->getLastWriteTime() == lastWriteTime)
            {
                stats.skips++;
                return false;
            }
        }

        return true; // need to scan
    }

    void ScanStepScanFiles::processFileScanResults(ScanContext& context, std::span<const FileScanResult> scanResults, const ScannerSettings::MediaLibraryInfo& libraryInfo)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ProcessScanResults");

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createWriteTransaction() };

        for (const FileScanResult& scanResult : scanResults)
        {
            if (_abortScan)
                return;

            if (const AudioFileScanData * scanData{ std::get_if<AudioFileScanData>(&scanResult.scanData) })
            {
                context.stats.scans++;
                processAudioFileScanData(context, scanResult.path, scanData->get(), libraryInfo);
            }
            else if (const ImageFileScanData * scanData{ std::get_if<ImageFileScanData>(&scanResult.scanData) })
            {
                context.stats.scans++;
                processImageFileScanData(context, scanResult.path, scanData->has_value() ? &scanData->value() : nullptr, libraryInfo);
            }
        }
    }

    void ScanStepScanFiles::processAudioFileScanData(ScanContext& context, const std::filesystem::path& file, const metadata::Track* trackMetadata, const ScannerSettings::MediaLibraryInfo& libraryInfo)
    {
        LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessAudioScanData");

        ScanStats& stats{ context.stats };

        const std::optional<FileInfo> fileInfo{ retrieveFileInfo(file, libraryInfo.rootDirectory) };
        if (!fileInfo)
        {
            stats.skips++;
            return;
        }

        db::Session& dbSession{ _db.getTLSSession() };
        Track::pointer track{ Track::findByPath(dbSession, file) };

        if (!trackMetadata)
        {
            if (track)
            {
                track.remove();
                stats.deletions++;
            }
            context.stats.errors.emplace_back(file, ScanErrorType::CannotReadAudioFile);
            return;
        }

        if (trackMetadata->mbid && (!track || _settings.skipDuplicateMBID))
        {
            std::vector<Track::pointer> duplicateTracks{ Track::findByMBID(dbSession, *trackMetadata->mbid) };

            // find for an existing track MBID as the file may have just been moved
            if (!track && duplicateTracks.size() == 1)
            {
                Track::pointer otherTrack{ duplicateTracks.front() };
                std::error_code ec;
                if (!std::filesystem::exists(otherTrack->getAbsoluteFilePath(), ec))
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Considering track '" << file.string() << "' moved from '" << otherTrack->getAbsoluteFilePath() << "'");
                    track = otherTrack;
                    track.modify()->setAbsoluteFilePath(file);
                }
            }

            // Skip duplicate track MBID
            if (_settings.skipDuplicateMBID)
            {
                for (Track::pointer& otherTrack : duplicateTracks)
                {
                    // Skip ourselves
                    if (track && track->getId() == otherTrack->getId())
                        continue;

                    // Skip if duplicate files no longer in media root: as it will be removed later, we will end up with no file
                    if (std::none_of(std::cbegin(_settings.mediaLibraries), std::cend(_settings.mediaLibraries),
                            [&](const ScannerSettings::MediaLibraryInfo& libraryInfo) {
                                return core::pathUtils::isPathInRootPath(file, libraryInfo.rootDirectory, &excludeDirFileName);
                            }))
                    {
                        continue;
                    }

                    LMS_LOG(DBUPDATER, DEBUG, "Skipped '" << file.string() << "' (similar MBID in '" << otherTrack->getAbsoluteFilePath().string() << "')");
                    // As this MBID already exists, just remove what we just scanned
                    if (track)
                    {
                        track.remove();
                        stats.deletions++;
                    }
                    return;
                }
            }
        }

        // We estimate this is an audio file if the duration is not null
        if (trackMetadata->audioProperties.duration == std::chrono::milliseconds::zero())
        {
            LMS_LOG(DBUPDATER, DEBUG, "Skipped '" << file.string() << "' (duration is 0)");

            // If Track exists here, delete it!
            if (track)
            {
                track.remove();
                stats.deletions++;
            }
            stats.errors.emplace_back(file, ScanErrorType::BadDuration);
            return;
        }

        // ***** Title
        std::string title;
        if (!trackMetadata->title.empty())
            title = trackMetadata->title;
        else
        {
            // TODO parse file name guess track etc.
            // For now juste use file name as title
            title = file.filename().string();
        }

        // If file already exists, update its data
        // Otherwise, create it
        bool added{};
        if (!track)
        {
            track = dbSession.create<Track>();
            track.modify()->setAbsoluteFilePath(file);
            added = true;
        }

        // Track related data
        assert(track);

        // Audio properties
        track.modify()->setBitrate(trackMetadata->audioProperties.bitrate);
        track.modify()->setBitsPerSample(trackMetadata->audioProperties.bitsPerSample);
        track.modify()->setChannelCount(trackMetadata->audioProperties.channelCount);
        track.modify()->setDuration(trackMetadata->audioProperties.duration);
        track.modify()->setSampleRate(trackMetadata->audioProperties.sampleRate);

        track.modify()->setRelativeFilePath(fileInfo->relativePath);
        track.modify()->setFileSize(fileInfo->fileSize);
        track.modify()->setLastWriteTime(fileInfo->lastWriteTime);

        MediaLibrary::pointer mediaLibrary{ MediaLibrary::find(dbSession, libraryInfo.id) }; // may be null if settings are updated in // => next scan will correct this
        track.modify()->setMediaLibrary(mediaLibrary);
        track.modify()->setDirectory(getOrCreateDirectory(dbSession, file.parent_path(), mediaLibrary));

        track.modify()->clearArtistLinks();
        // Do not fallback on artists with the same name but having a MBID for artist and releaseArtists, as it may be corrected by properly tagging files
        for (const Artist::pointer& artist : getOrCreateArtists(dbSession, trackMetadata->artists, false))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, artist, TrackArtistLinkType::Artist));

        if (trackMetadata->medium && trackMetadata->medium->release)
        {
            for (const Artist::pointer& releaseArtist : getOrCreateArtists(dbSession, trackMetadata->medium->release->artists, false))
                track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, releaseArtist, TrackArtistLinkType::ReleaseArtist));
        }

        // Allow fallbacks on artists with the same name even if they have MBID, since there is no tag to indicate the MBID of these artists
        // We could ask MusicBrainz to get all the information, but that would heavily slow down the import process
        for (const Artist::pointer& conductor : getOrCreateArtists(dbSession, trackMetadata->conductorArtists, true))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, conductor, TrackArtistLinkType::Conductor));

        for (const Artist::pointer& composer : getOrCreateArtists(dbSession, trackMetadata->composerArtists, true))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, composer, TrackArtistLinkType::Composer));

        for (const Artist::pointer& lyricist : getOrCreateArtists(dbSession, trackMetadata->lyricistArtists, true))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, lyricist, TrackArtistLinkType::Lyricist));

        for (const Artist::pointer& mixer : getOrCreateArtists(dbSession, trackMetadata->mixerArtists, true))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, mixer, TrackArtistLinkType::Mixer));

        for (const auto& [role, performers] : trackMetadata->performerArtists)
        {
            for (const Artist::pointer& performer : getOrCreateArtists(dbSession, performers, true))
                track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, performer, TrackArtistLinkType::Performer, role));
        }

        for (const Artist::pointer& producer : getOrCreateArtists(dbSession, trackMetadata->producerArtists, true))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, producer, TrackArtistLinkType::Producer));

        for (const Artist::pointer& remixer : getOrCreateArtists(dbSession, trackMetadata->remixerArtists, true))
            track.modify()->addArtistLink(TrackArtistLink::create(dbSession, track, remixer, TrackArtistLinkType::Remixer));

        track.modify()->setScanVersion(_settings.scanVersion);
        if (trackMetadata->medium && trackMetadata->medium->release)
            track.modify()->setRelease(getOrCreateRelease(dbSession, *trackMetadata->medium->release, file.parent_path()));
        else
            track.modify()->setRelease({});
        track.modify()->setTotalTrack(trackMetadata->medium ? trackMetadata->medium->trackCount : std::nullopt);
        track.modify()->setReleaseReplayGain(trackMetadata->medium ? trackMetadata->medium->replayGain : std::nullopt);
        track.modify()->setDiscSubtitle(trackMetadata->medium ? trackMetadata->medium->name : "");
        track.modify()->setClusters(getOrCreateClusters(dbSession, *trackMetadata));
        track.modify()->setName(title);
        track.modify()->setAddedTime(Wt::WDateTime::currentDateTime());
        track.modify()->setTrackNumber(trackMetadata->position);
        track.modify()->setDiscNumber(trackMetadata->medium ? trackMetadata->medium->position : std::nullopt);
        track.modify()->setDate(trackMetadata->date);
        track.modify()->setYear(trackMetadata->year);
        track.modify()->setOriginalDate(trackMetadata->originalDate);
        track.modify()->setOriginalYear(trackMetadata->originalYear);

        // If a file has an OriginalDate but no date, set it to ease filtering
        if (!trackMetadata->date.isValid() && trackMetadata->originalDate.isValid())
            track.modify()->setDate(trackMetadata->originalDate);

        // If a file has an OriginalYear but no Year, set it to ease filtering
        if (!trackMetadata->year && trackMetadata->originalYear)
            track.modify()->setYear(trackMetadata->originalYear);

        track.modify()->setRecordingMBID(trackMetadata->recordingMBID);
        track.modify()->setTrackMBID(trackMetadata->mbid);
        if (auto trackFeatures{ TrackFeatures::find(dbSession, track->getId()) })
            trackFeatures.remove(); // TODO: only if MBID changed?
        track.modify()->setHasCover(trackMetadata->hasCover);
        track.modify()->setCopyright(trackMetadata->copyright);
        track.modify()->setCopyrightURL(trackMetadata->copyrightURL);
        track.modify()->setTrackReplayGain(trackMetadata->replayGain);
        track.modify()->setArtistDisplayName(trackMetadata->artistDisplayName);

        if (added)
        {
            LMS_LOG(DBUPDATER, DEBUG, "Added audio file '" << file.string() << "'");
            stats.additions++;
        }
        else
        {
            LMS_LOG(DBUPDATER, DEBUG, "Updated audio file '" << file.string() << "'");
            stats.updates++;
        }
    }

    void ScanStepScanFiles::processImageFileScanData(ScanContext& context, const std::filesystem::path& file, const ImageInfo* imageInfo, const ScannerSettings::MediaLibraryInfo& libraryInfo)
    {
        LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessImageScanData");

        ScanStats& stats{ context.stats };

        const std::optional<FileInfo> fileInfo{ retrieveFileInfo(file, libraryInfo.rootDirectory) };
        if (!fileInfo)
        {
            stats.skips++;
            return;
        }

        db::Session& dbSession{ _db.getTLSSession() };
        db::Image::pointer image{ db::Image::find(dbSession, file) };

        if (!imageInfo)
        {
            if (image)
            {
                image.remove();
                stats.deletions++;
            }
            context.stats.errors.emplace_back(file, ScanErrorType::CannotReadImageFile);
            return;
        }

        bool added;
        if (!image)
        {
            image = dbSession.create<db::Image>(file);
            added = true;
        }
        else
        {
            added = false;
        }

        image.modify()->setLastWriteTime(fileInfo->lastWriteTime);
        image.modify()->setFileSize(fileInfo->fileSize);
        image.modify()->setHeight(imageInfo->height);
        image.modify()->setWidth(imageInfo->width);
        MediaLibrary::pointer mediaLibrary{ MediaLibrary::find(dbSession, libraryInfo.id) }; // may be null if settings are updated in // => next scan will correct this
        image.modify()->setDirectory(getOrCreateDirectory(dbSession, file.parent_path(), mediaLibrary));

        if (added)
        {
            LMS_LOG(DBUPDATER, DEBUG, "Added image '" << file.string() << "'");
            stats.additions++;
        }
        else
        {
            LMS_LOG(DBUPDATER, DEBUG, "Updated image '" << file.string() << "'");
            stats.updates++;
        }
    }

    void ScanStepScanFiles::updateDirectoryIfNeeded(const std::filesystem::path& dirPath, const ScannerSettings::MediaLibraryInfo& libraryInfo)
    {
        db::Session& dbSession{ _db.getTLSSession() };

        const bool needUpdateLibrary{ [&] {
            auto transaction{ dbSession.createReadTransaction() };

            Directory::pointer directory{ Directory::find(dbSession, dirPath) };
            if (!directory)
                return false; // we create directories only of we find images or tracks inside

            MediaLibrary::pointer currentLibrary{ directory->getMediaLibrary() };
            return !currentLibrary || currentLibrary->getId() != libraryInfo.id;
        }() };

        if (needUpdateLibrary)
        {
            auto transaction{ dbSession.createWriteTransaction() };

            Directory::pointer directory{ Directory::find(dbSession, dirPath) };
            assert(directory);
            directory.modify()->setMediaLibrary(MediaLibrary::find(dbSession, libraryInfo.id)); // may be null if settings are updated in // => next scan will correct this
        }

        if (dirPath != libraryInfo.rootDirectory && dirPath.has_parent_path())
        {
            updateDirectoryIfNeeded(dirPath.parent_path(), libraryInfo);
        }
    }
} // namespace lms::scanner
