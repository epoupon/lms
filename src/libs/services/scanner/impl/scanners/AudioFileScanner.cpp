/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "AudioFileScanner.hpp"

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Path.hpp"
#include "core/Service.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/TrackFeatures.hpp"
#include "database/TrackLyrics.hpp"
#include "metadata/Exception.hpp"
#include "metadata/IParser.hpp"

#include "IFileScanOperation.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"
#include "Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        db::Artist::pointer createArtist(db::Session& session, const metadata::Artist& artistInfo)
        {
            db::Artist::pointer artist{ session.create<db::Artist>(artistInfo.name) };

            if (artistInfo.mbid)
                artist.modify()->setMBID(artistInfo.mbid);
            if (artistInfo.sortName)
                artist.modify()->setSortName(*artistInfo.sortName);

            return artist;
        }

        std::string optionalMBIDAsString(const std::optional<core::UUID>& uuid)
        {
            return uuid ? std::string{ uuid->getAsString() } : "<no MBID>";
        }

        void updateArtistIfNeeded(db::Artist::pointer artist, const metadata::Artist& artistInfo)
        {
            // Name may have been updated
            if (artist->getName() != artistInfo.name)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Artist [" << optionalMBIDAsString(artist->getMBID()) << "], updated name from '" << artist->getName() << "' to '" << artistInfo.name << "'");
                artist.modify()->setName(artistInfo.name);
            }

            // Sortname may have been updated
            // As the sort name is quite often not filled in, we update it only if already set (for now?)
            if (artistInfo.sortName && *artistInfo.sortName != artist->getSortName())
            {
                LMS_LOG(DBUPDATER, DEBUG, "Artist [" << optionalMBIDAsString(artist->getMBID()) << "], updated sort name from '" << artist->getSortName() << "' to '" << *artistInfo.sortName << "'");
                artist.modify()->setSortName(*artistInfo.sortName);
            }
        }

        std::vector<db::Artist::pointer> getOrCreateArtists(db::Session& session, const std::vector<metadata::Artist>& artistsInfo, bool allowFallbackOnMBIDEntries)
        {
            std::vector<db::Artist::pointer> artists;

            for (const metadata::Artist& artistInfo : artistsInfo)
            {
                db::Artist::pointer artist;

                // First try to get by MBID
                if (artistInfo.mbid)
                {
                    artist = db::Artist::find(session, *artistInfo.mbid);
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
                    for (const db::Artist::pointer& sameNamedArtist : db::Artist::find(session, artistInfo.name))
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

        db::ReleaseType::pointer getOrCreateReleaseType(db::Session& session, std::string_view name)
        {
            db::ReleaseType::pointer releaseType{ db::ReleaseType::find(session, name) };
            if (!releaseType)
                releaseType = session.create<db::ReleaseType>(name);

            return releaseType;
        }

        db::Label::pointer getOrCreateLabel(db::Session& session, std::string_view name)
        {
            db::Label::pointer label{ db::Label::find(session, name) };
            if (!label)
                label = session.create<db::Label>(name);

            return label;
        }

        void updateReleaseIfNeeded(db::Session& session, db::Release::pointer release, const metadata::Release& releaseInfo)
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
            if (release->isCompilation() != releaseInfo.isCompilation)
                release.modify()->setCompilation(releaseInfo.isCompilation);
            if (release->getBarcode() != releaseInfo.barcode)
                release.modify()->setBarcode(releaseInfo.barcode);
            if (release->getReleaseTypeNames() != releaseInfo.releaseTypes)
            {
                release.modify()->clearReleaseTypes();
                for (std::string_view releaseType : releaseInfo.releaseTypes)
                    release.modify()->addReleaseType(getOrCreateReleaseType(session, releaseType));
            }

            if (release->getLabelNames() != releaseInfo.labels)
            {
                release.modify()->clearLabels();
                for (std::string_view label : releaseInfo.labels)
                    release.modify()->addLabel(getOrCreateLabel(session, label));
            }
        }

        // Compare release level info
        bool isReleaseMatching(const db::Release::pointer& candidateRelease, const metadata::Release& releaseInfo)
        {
            // TODO: add more criterias?
            return candidateRelease->getName() == releaseInfo.name
                && candidateRelease->getSortName() == releaseInfo.sortName
                && candidateRelease->getTotalDisc() == releaseInfo.mediumCount
                && candidateRelease->isCompilation() == releaseInfo.isCompilation
                && candidateRelease->getLabelNames() == releaseInfo.labels
                && candidateRelease->getBarcode() == releaseInfo.barcode;
        }

        db::Release::pointer getOrCreateRelease(db::Session& session, const metadata::Release& releaseInfo, const db::Directory::pointer& currentDirectory)
        {
            db::Release::pointer release;

            // First try to get by MBID: fastest, safest
            if (releaseInfo.mbid)
            {
                release = db::Release::find(session, *releaseInfo.mbid);
                if (!release)
                    release = session.create<db::Release>(releaseInfo.name, releaseInfo.mbid);
            }
            else if (releaseInfo.name.empty())
            {
                // No release name (only mbid) -> nothing to do
                return release;
            }

            // Fall back on release name (collisions may occur)
            // First try using all sibling directories (case for Album/DiscX), only if the disc number is set
            const db::DirectoryId parentDirectoryId{ currentDirectory->getParentDirectoryId() };
            if (!release && releaseInfo.mediumCount && *releaseInfo.mediumCount > 1 && parentDirectoryId.isValid())
            {
                db::Release::FindParameters params;
                params.setParentDirectory(parentDirectoryId);
                params.setName(releaseInfo.name);
                db::Release::find(session, params, [&](const db::Release::pointer& candidateRelease) {
                    // Already found a candidate
                    if (release)
                        return;

                    // Do not fallback on properly tagged releases
                    if (candidateRelease->getMBID().has_value())
                        return;

                    if (!isReleaseMatching(candidateRelease, releaseInfo))
                        return;

                    release = candidateRelease;
                });
            }

            // Lastly try in the current directory: we do this at last to have
            // opportunities to merge releases in case of migration / rescan
            if (!release)
            {
                db::Release::FindParameters params;
                params.setDirectory(currentDirectory->getId());
                params.setName(releaseInfo.name);
                db::Release::find(session, params, [&](const db::Release::pointer& candidateRelease) {
                    // Already found a candidate
                    if (release)
                        return;

                    // Do not fallback on properly tagged releases
                    if (candidateRelease->getMBID().has_value())
                        return;

                    if (!isReleaseMatching(candidateRelease, releaseInfo))
                        return;

                    release = candidateRelease;
                });
            }

            if (!release)
                release = session.create<db::Release>(releaseInfo.name);

            updateReleaseIfNeeded(session, release, releaseInfo);
            return release;
        }

        std::vector<db::Cluster::pointer> getOrCreateClusters(db::Session& session, const metadata::Track& track)
        {
            std::vector<db::Cluster::pointer> clusters;

            auto getOrCreateClusters{ [&](std::string_view tag, std::span<const std::string> values) {
                auto clusterType = db::ClusterType::find(session, tag);
                if (!clusterType)
                    clusterType = session.create<db::ClusterType>(tag);

                for (const auto& value : values)
                {
                    auto cluster{ clusterType->getCluster(value) };
                    if (!cluster)
                        cluster = session.create<db::Cluster>(clusterType, value);

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

        db::TrackLyrics::pointer createLyrics(db::Session& session, const metadata::Lyrics& lyricsInfo)
        {
            db::TrackLyrics::pointer lyrics{ session.create<db::TrackLyrics>() };

            lyrics.modify()->setLanguage(!lyricsInfo.language.empty() ? lyricsInfo.language : "xxx");
            lyrics.modify()->setOffset(lyricsInfo.offset);
            lyrics.modify()->setDisplayArtist(lyricsInfo.displayArtist);
            lyrics.modify()->setDisplayTitle(lyricsInfo.displayTitle);
            if (!lyricsInfo.synchronizedLines.empty())
                lyrics.modify()->setSynchronizedLines(lyricsInfo.synchronizedLines);
            else
                lyrics.modify()->setUnsynchronizedLines(lyricsInfo.unsynchronizedLines);

            return lyrics;
        }

        class AudioFileScanOperation : public IFileScanOperation
        {
        public:
            AudioFileScanOperation(const FileToScan& fileToScan, db::Db& db, metadata::IParser& parser, const ScannerSettings& settings)
                : _file{ fileToScan.file }
                , _mediaLibrary{ fileToScan.mediaLibrary }
                , _db{ db }
                , _parser{ parser }
                , _settings{ settings }
            {
            }
            ~AudioFileScanOperation() override = default;
            AudioFileScanOperation(const AudioFileScanOperation&) = delete;
            AudioFileScanOperation& operator=(const AudioFileScanOperation&) = delete;

        private:
            const std::filesystem::path& getFile() const override { return _file; };
            core::LiteralString getName() const override { return "ScanAudioFile"; }
            void scan() override;
            void processResult(ScanContext& context) override;

            const std::filesystem::path _file;
            const MediaLibraryInfo _mediaLibrary;
            db::Db& _db;
            metadata::IParser& _parser;
            const ScannerSettings& _settings;
            std::unique_ptr<metadata::Track> _parsedTrack;
        };

        void AudioFileScanOperation::scan()
        {
            LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ScanAudioFile");
            std::unique_ptr<metadata::Track> track;

            try
            {
                _parsedTrack = _parser.parse(_file);
            }
            catch (const metadata::Exception& e)
            {
                LMS_LOG(DBUPDATER, INFO, "Failed to parse audio file '" << _file.string() << "'");
            }
        }

        void AudioFileScanOperation::processResult(ScanContext& context)
        {
            LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessAudioScanData");

            ScanStats& stats{ context.stats };

            const std::optional<FileInfo> fileInfo{ utils::retrieveFileInfo(_file, _mediaLibrary.rootDirectory) };
            if (!fileInfo)
            {
                stats.skips++;
                return;
            }

            db::Session& dbSession{ _db.getTLSSession() };
            db::Track::pointer track{ db::Track::findByPath(dbSession, _file) };

            if (!_parsedTrack)
            {
                if (track)
                {
                    track.remove();
                    stats.deletions++;
                }
                context.stats.errors.emplace_back(_file, ScanErrorType::CannotReadAudioFile);
                return;
            }

            if (_parsedTrack->mbid && (!track || _settings.skipDuplicateMBID))
            {
                std::vector<db::Track::pointer> duplicateTracks{ db::Track::findByMBID(dbSession, *_parsedTrack->mbid) };

                // find for an existing track MBID as the file may have just been moved
                if (!track && duplicateTracks.size() == 1)
                {
                    db::Track::pointer otherTrack{ duplicateTracks.front() };
                    std::error_code ec;
                    if (!std::filesystem::exists(otherTrack->getAbsoluteFilePath(), ec))
                    {
                        LMS_LOG(DBUPDATER, DEBUG, "Considering track '" << _file.string() << "' moved from '" << otherTrack->getAbsoluteFilePath() << "'");
                        track = otherTrack;
                        track.modify()->setAbsoluteFilePath(_file);
                    }
                }

                // Skip duplicate track MBID
                if (_settings.skipDuplicateMBID)
                {
                    for (db::Track::pointer& otherTrack : duplicateTracks)
                    {
                        // Skip ourselves
                        if (track && track->getId() == otherTrack->getId())
                            continue;

                        // Skip if duplicate files no longer in media root: as it will be removed later, we will end up with no file
                        if (std::none_of(std::cbegin(_settings.mediaLibraries), std::cend(_settings.mediaLibraries),
                                [&](const MediaLibraryInfo& libraryInfo) {
                                    return core::pathUtils::isPathInRootPath(_file, libraryInfo.rootDirectory, &excludeDirFileName);
                                }))
                        {
                            continue;
                        }

                        LMS_LOG(DBUPDATER, DEBUG, "Skipped '" << _file.string() << "' (similar MBID in '" << otherTrack->getAbsoluteFilePath().string() << "')");
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
            if (_parsedTrack->audioProperties.duration == std::chrono::milliseconds::zero())
            {
                LMS_LOG(DBUPDATER, DEBUG, "Skipped '" << _file.string() << "' (duration is 0)");

                // If Track exists here, delete it!
                if (track)
                {
                    track.remove();
                    stats.deletions++;
                }
                stats.errors.emplace_back(_file, ScanErrorType::BadDuration);
                return;
            }

            // ***** Title
            std::string title;
            if (!_parsedTrack->title.empty())
                title = _parsedTrack->title;
            else
            {
                // TODO parse file name guess track etc.
                // For now juste use file name as title
                title = _file.filename().string();
            }

            // If file already exists, update its data
            // Otherwise, create it
            bool added{};
            if (!track)
            {
                track = dbSession.create<db::Track>();
                track.modify()->setAbsoluteFilePath(_file);
                added = true;
            }

            // Track related data
            assert(track);

            // Audio properties
            track.modify()->setBitrate(_parsedTrack->audioProperties.bitrate);
            track.modify()->setBitsPerSample(_parsedTrack->audioProperties.bitsPerSample);
            track.modify()->setChannelCount(_parsedTrack->audioProperties.channelCount);
            track.modify()->setDuration(_parsedTrack->audioProperties.duration);
            track.modify()->setSampleRate(_parsedTrack->audioProperties.sampleRate);

            track.modify()->setRelativeFilePath(fileInfo->relativePath);
            track.modify()->setFileSize(fileInfo->fileSize);
            track.modify()->setLastWriteTime(fileInfo->lastWriteTime);

            db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, _mediaLibrary.id) }; // may be null if settings are updated in // => next scan will correct this
            track.modify()->setMediaLibrary(mediaLibrary);
            db::Directory::pointer directory{ utils::getOrCreateDirectory(dbSession, _file.parent_path(), mediaLibrary) };
            track.modify()->setDirectory(directory);

            track.modify()->clearArtistLinks();
            // Do not fallback on artists with the same name but having a MBID for artist and releaseArtists, as it may be corrected by properly tagging files
            for (const db::Artist::pointer& artist : getOrCreateArtists(dbSession, _parsedTrack->artists, false))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, artist, db::TrackArtistLinkType::Artist));

            if (_parsedTrack->medium && _parsedTrack->medium->release)
            {
                for (const db::Artist::pointer& releaseArtist : getOrCreateArtists(dbSession, _parsedTrack->medium->release->artists, false))
                    track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, releaseArtist, db::TrackArtistLinkType::ReleaseArtist));
            }

            // Allow fallbacks on artists with the same name even if they have MBID, since there is no tag to indicate the MBID of these artists
            // We could ask MusicBrainz to get all the information, but that would heavily slow down the import process
            for (const db::Artist::pointer& conductor : getOrCreateArtists(dbSession, _parsedTrack->conductorArtists, true))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, conductor, db::TrackArtistLinkType::Conductor));

            for (const db::Artist::pointer& composer : getOrCreateArtists(dbSession, _parsedTrack->composerArtists, true))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, composer, db::TrackArtistLinkType::Composer));

            for (const db::Artist::pointer& lyricist : getOrCreateArtists(dbSession, _parsedTrack->lyricistArtists, true))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, lyricist, db::TrackArtistLinkType::Lyricist));

            for (const db::Artist::pointer& mixer : getOrCreateArtists(dbSession, _parsedTrack->mixerArtists, true))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, mixer, db::TrackArtistLinkType::Mixer));

            for (const auto& [role, performers] : _parsedTrack->performerArtists)
            {
                for (const db::Artist::pointer& performer : getOrCreateArtists(dbSession, performers, true))
                    track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, performer, db::TrackArtistLinkType::Performer, role));
            }

            for (const db::Artist::pointer& producer : getOrCreateArtists(dbSession, _parsedTrack->producerArtists, true))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, producer, db::TrackArtistLinkType::Producer));

            for (const db::Artist::pointer& remixer : getOrCreateArtists(dbSession, _parsedTrack->remixerArtists, true))
                track.modify()->addArtistLink(db::TrackArtistLink::create(dbSession, track, remixer, db::TrackArtistLinkType::Remixer));

            track.modify()->setScanVersion(_settings.scanVersion);
            if (_parsedTrack->medium && _parsedTrack->medium->release)
                track.modify()->setRelease(getOrCreateRelease(dbSession, *_parsedTrack->medium->release, directory));
            else
                track.modify()->setRelease({});
            track.modify()->setTotalTrack(_parsedTrack->medium ? _parsedTrack->medium->trackCount : std::nullopt);
            track.modify()->setReleaseReplayGain(_parsedTrack->medium ? _parsedTrack->medium->replayGain : std::nullopt);
            track.modify()->setDiscSubtitle(_parsedTrack->medium ? _parsedTrack->medium->name : "");
            track.modify()->setClusters(getOrCreateClusters(dbSession, *_parsedTrack));
            track.modify()->setName(title);
            track.modify()->setAddedTime(Wt::WDateTime::currentDateTime());
            track.modify()->setTrackNumber(_parsedTrack->position);
            track.modify()->setDiscNumber(_parsedTrack->medium ? _parsedTrack->medium->position : std::nullopt);
            track.modify()->setDate(_parsedTrack->date);
            track.modify()->setYear(_parsedTrack->year);
            track.modify()->setOriginalDate(_parsedTrack->originalDate);
            track.modify()->setOriginalYear(_parsedTrack->originalYear);

            // If a file has an OriginalDate but no date, set it to ease filtering
            if (!_parsedTrack->date.isValid() && _parsedTrack->originalDate.isValid())
                track.modify()->setDate(_parsedTrack->originalDate);

            // If a file has an OriginalYear but no Year, set it to ease filtering
            if (!_parsedTrack->year && _parsedTrack->originalYear)
                track.modify()->setYear(_parsedTrack->originalYear);

            track.modify()->setRecordingMBID(_parsedTrack->recordingMBID);
            track.modify()->setTrackMBID(_parsedTrack->mbid);
            if (auto trackFeatures{ db::TrackFeatures::find(dbSession, track->getId()) })
                trackFeatures.remove(); // TODO: only if MBID changed?
            track.modify()->setHasCover(_parsedTrack->hasCover);
            track.modify()->setCopyright(_parsedTrack->copyright);
            track.modify()->setCopyrightURL(_parsedTrack->copyrightURL);
            track.modify()->setComment(!_parsedTrack->comments.empty() ? _parsedTrack->comments.front() : ""); // only take the first one for now
            track.modify()->setTrackReplayGain(_parsedTrack->replayGain);
            track.modify()->setArtistDisplayName(_parsedTrack->artistDisplayName);

            track.modify()->clearEmbeddedLyrics();
            for (const metadata::Lyrics& lyricsInfo : _parsedTrack->lyrics)
            {
                db::TrackLyrics::pointer lyrics{ createLyrics(dbSession, lyricsInfo) };
                track.modify()->addLyrics(lyrics);
            }

            if (added)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Added audio file '" << _file.string() << "'");
                stats.additions++;
            }
            else
            {
                LMS_LOG(DBUPDATER, DEBUG, "Updated audio file '" << _file.string() << "'");
                stats.updates++;
            }
        }

        metadata::ParserReadStyle getParserReadStyle()
        {
            std::string_view readStyle{ core::Service<core::IConfig>::get()->getString("scanner-parser-read-style", "average") };

            if (readStyle == "fast")
                return metadata::ParserReadStyle::Fast;
            if (readStyle == "average")
                return metadata::ParserReadStyle::Average;
            if (readStyle == "accurate")
                return metadata::ParserReadStyle::Accurate;

            throw core::LmsException{ "Invalid value for 'scanner-parser-read-style'" };
        }
    } // namespace

    AudioFileScanner::AudioFileScanner(db::Db& db, const ScannerSettings& settings)
        : _db{ db }
        , _settings{ settings }
        , _metadataParser{ metadata::createParser(metadata::ParserBackend::TagLib, getParserReadStyle()) } // For now, always use TagLib
    {
        std::vector<std::string> tagsToParse{ _extraTagsToParse };
        tagsToParse.insert(std::end(tagsToParse), std::cbegin(settings.extraTags), std::cend(settings.extraTags));
        _metadataParser->setUserExtraTags(tagsToParse);
        _metadataParser->setArtistTagDelimiters(settings.artistTagDelimiters);
        _metadataParser->setDefaultTagDelimiters(settings.defaultTagDelimiters);
    }

    AudioFileScanner::~AudioFileScanner() = default;

    core::LiteralString AudioFileScanner::getName() const
    {
        return "Audio scanner";
    }

    std::span<const std::filesystem::path> AudioFileScanner::getSupportedExtensions() const
    {
        return _metadataParser->getSupportedExtensions();
    }

    bool AudioFileScanner::needsScan(ScanContext& context, const FileToScan& file) const
    {
        ScanStats& stats{ context.stats };

        const Wt::WDateTime lastWriteTime{ utils::retrieveFileGetLastWrite(file.file) };
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
            const db::Track::pointer track{ db::Track::findByPath(dbSession, file.file) };
            if (track
                && track->getLastWriteTime() == lastWriteTime
                && track->getScanVersion() == _settings.scanVersion)
            {
                // this file may have been moved from one library to another, then we just need to update the media library id instead of a full rescan
                const auto trackMediaLibrary{ track->getMediaLibrary() };
                if (trackMediaLibrary && trackMediaLibrary->getId() == file.mediaLibrary.id)
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

            db::Track::pointer track{ db::Track::findByPath(dbSession, file.file) };
            assert(track);
            track.modify()->setMediaLibrary(db::MediaLibrary::find(dbSession, file.mediaLibrary.id)); // may be null, will be handled in the next scan anyway
            stats.updates++;
            return false;
        }

        return true; // need to scan
    }

    std::unique_ptr<IFileScanOperation> AudioFileScanner::createScanOperation(const FileToScan& fileToScan) const
    {
        return std::make_unique<AudioFileScanOperation>(fileToScan, _db, *_metadataParser, _settings);
    }
} // namespace lms::scanner
