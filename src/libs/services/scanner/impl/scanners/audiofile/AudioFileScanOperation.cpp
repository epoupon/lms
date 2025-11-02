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

#include "AudioFileScanOperation.hpp"

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/PartialDateTime.hpp"
#include "core/Path.hpp"
#include "core/XxHash3.hpp"

#include "audio/IAudioFileInfo.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "database/objects/TrackLyrics.hpp"

#include "services/scanner/ScanErrors.hpp"

#include "ScannerSettings.hpp"
#include "helpers/ArtistHelpers.hpp"
#include "scanners/IFileScanOperation.hpp"
#include "scanners/Utils.hpp"
#include "scanners/audiofile/TrackMetadataParser.hpp"

namespace lms::scanner
{
    namespace
    {
        void createTrackArtistLinks(db::Session& session, const db::Track::pointer& track, db::TrackArtistLinkType linkType, std::string_view role, std::span<const Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
        {
            for (const Artist& artist : artists)
            {
                db::Artist::pointer dbArtist{ helpers::getOrCreateArtist(session, artist, allowArtistMBIDFallback) };

                const bool matchedUsingMbid{ artist.mbid.has_value() && dbArtist->getMBID() == artist.mbid };
                db::TrackArtistLink::pointer link{ session.create<db::TrackArtistLink>(track, dbArtist, linkType, role, matchedUsingMbid) };
                link.modify()->setArtistName(artist.name);
                if (artist.sortName)
                    link.modify()->setArtistSortName(*artist.sortName);
            }
        }

        void createTrackArtistLinks(db::Session& session, const db::Track::pointer& track, db::TrackArtistLinkType linkType, std::span<const Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
        {
            constexpr std::string_view noRole{};
            createTrackArtistLinks(session, track, linkType, noRole, artists, allowArtistMBIDFallback);
        }

        db::ReleaseType::pointer getOrCreateReleaseType(db::Session& session, std::string_view name)
        {
            db::ReleaseType::pointer releaseType{ db::ReleaseType::find(session, name) };
            if (!releaseType)
                releaseType = session.create<db::ReleaseType>(name);

            return releaseType;
        }

        db::Country::pointer getOrCreateCountry(db::Session& session, std::string_view name)
        {
            db::Country::pointer country{ db::Country::find(session, name) };
            if (!country)
                country = session.create<db::Country>(name);

            return country;
        }

        db::Label::pointer getOrCreateLabel(db::Session& session, std::string_view name)
        {
            db::Label::pointer label{ db::Label::find(session, name) };
            if (!label)
                label = session.create<db::Label>(name);

            return label;
        }

        void updateReleaseIfNeeded(db::Session& session, db::Release::pointer dbRelease, const Release& release)
        {
            if (dbRelease->getName() != release.name)
                dbRelease.modify()->setName(release.name);
            if (dbRelease->getSortName() != release.sortName)
                dbRelease.modify()->setSortName(release.sortName);
            if (dbRelease->getGroupMBID() != release.groupMBID)
                dbRelease.modify()->setGroupMBID(release.groupMBID);
            if (dbRelease->getTotalDisc() != release.mediumCount)
                dbRelease.modify()->setTotalDisc(release.mediumCount);
            if (dbRelease->getArtistDisplayName() != release.artistDisplayName)
                dbRelease.modify()->setArtistDisplayName(release.artistDisplayName);
            if (dbRelease->isCompilation() != release.isCompilation)
                dbRelease.modify()->setCompilation(release.isCompilation);
            if (dbRelease->getBarcode() != release.barcode)
                dbRelease.modify()->setBarcode(release.barcode);
            if (dbRelease->getComment() != release.comment)
                dbRelease.modify()->setComment(release.comment);
            if (dbRelease->getReleaseTypeNames() != release.releaseTypes)
            {
                dbRelease.modify()->clearReleaseTypes();
                for (std::string_view releaseType : release.releaseTypes)
                    dbRelease.modify()->addReleaseType(getOrCreateReleaseType(session, releaseType));
            }
            if (dbRelease->getCountryNames() != release.countries)
            {
                dbRelease.modify()->clearCountries();
                for (std::string_view country : release.countries)
                    dbRelease.modify()->addCountry(getOrCreateCountry(session, country));
            }
            if (dbRelease->getLabelNames() != release.labels)
            {
                dbRelease.modify()->clearLabels();
                for (std::string_view label : release.labels)
                    dbRelease.modify()->addLabel(getOrCreateLabel(session, label));
            }
        }

        // Compare release level info
        bool isReleaseMatching(const db::Release::pointer& dbCandidateRelease, const Release& release)
        {
            // TODO: add more criterias?
            return dbCandidateRelease->getName() == release.name
                && dbCandidateRelease->getSortName() == release.sortName
                && dbCandidateRelease->getTotalDisc() == release.mediumCount
                && dbCandidateRelease->isCompilation() == release.isCompilation
                && dbCandidateRelease->getLabelNames() == release.labels
                && dbCandidateRelease->getBarcode() == release.barcode;
        }

        db::Release::pointer getOrCreateRelease(db::Session& session, const Release& release, const db::Directory::pointer& currentDirectory)
        {
            db::Release::pointer dbRelease;

            // First try to get by MBID: fastest, safest
            if (release.mbid)
            {
                dbRelease = db::Release::find(session, *release.mbid);
                if (!dbRelease)
                    dbRelease = session.create<db::Release>(release.name, release.mbid);
            }
            else if (release.name.empty())
            {
                // No release name (only mbid) -> nothing to do
                return dbRelease;
            }

            // Fall back on release name (collisions may occur)
            // First try using all sibling directories (case for Album/DiscX), only if the disc number is set
            const db::DirectoryId parentDirectoryId{ currentDirectory->getParentDirectoryId() };
            if (!dbRelease && release.mediumCount && *release.mediumCount > 1 && parentDirectoryId.isValid())
            {
                db::Release::FindParameters params;
                params.setParentDirectory(parentDirectoryId);
                params.setName(release.name);
                db::Release::find(session, params, [&](const db::Release::pointer& dbCandidateRelease) {
                    // Already found a candidate
                    if (dbRelease)
                        return;

                    // Do not fallback on properly tagged releases
                    if (dbCandidateRelease->getMBID().has_value())
                        return;

                    if (!isReleaseMatching(dbCandidateRelease, release))
                        return;

                    dbRelease = dbCandidateRelease;
                });
            }

            // Lastly try in the current directory: we do this at last to have
            // opportunities to merge releases in case of migration / rescan
            if (!dbRelease)
            {
                db::Release::FindParameters params;
                params.setDirectory(currentDirectory->getId());
                params.setName(release.name);
                db::Release::find(session, params, [&](const db::Release::pointer& dbCandidateRelease) {
                    // Already found a candidate
                    if (dbRelease)
                        return;

                    // Do not fallback on properly tagged releases
                    if (dbCandidateRelease->getMBID().has_value())
                        return;

                    if (!isReleaseMatching(dbCandidateRelease, release))
                        return;

                    dbRelease = dbCandidateRelease;
                });
            }

            if (!dbRelease)
                dbRelease = session.create<db::Release>(release.name);

            updateReleaseIfNeeded(session, dbRelease, release);
            return dbRelease;
        }

        db::Medium::pointer getOrCreateMedium(db::Session& session, const Medium& medium, const db::Release::pointer& release)
        {
            db::Medium::pointer dbMedium{ db::Medium::find(session, release->getId(), medium.position) };
            if (!dbMedium)
                dbMedium = session.create<db::Medium>(release);

            if (dbMedium->getPosition() != medium.position)
                dbMedium.modify()->setPosition(medium.position);
            if (dbMedium->getMedia() != medium.media)
                dbMedium.modify()->setMedia(medium.media);
            if (dbMedium->getName() != medium.name)
                dbMedium.modify()->setName(medium.name);
            if (dbMedium->getTrackCount() != medium.trackCount)
                dbMedium.modify()->setTrackCount(medium.trackCount);
            if (dbMedium->getReplayGain() != medium.replayGain)
                dbMedium.modify()->setReplayGain(medium.replayGain);

            return dbMedium;
        }

        std::vector<db::Cluster::pointer> getOrCreateClusters(db::Session& session, const Track& track)
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

        db::TrackLyrics::pointer createLyrics(db::Session& session, const Lyrics& lyrics)
        {
            db::TrackLyrics::pointer dbLyrics{ session.create<db::TrackLyrics>() };

            dbLyrics.modify()->setLanguage(!lyrics.language.empty() ? lyrics.language : "xxx");
            dbLyrics.modify()->setOffset(lyrics.offset);
            dbLyrics.modify()->setDisplayArtist(lyrics.displayArtist);
            dbLyrics.modify()->setDisplayTitle(lyrics.displayTitle);
            if (!lyrics.synchronizedLines.empty())
                dbLyrics.modify()->setSynchronizedLines(lyrics.synchronizedLines);
            else
                dbLyrics.modify()->setUnsynchronizedLines(lyrics.unsynchronizedLines);

            return dbLyrics;
        }

        db::ImageType convertImageType(audio::Image::Type type)
        {
            switch (type)
            {
            case audio::Image::Type::Unknown:
                return db::ImageType::Unknown;
            case audio::Image::Type::Other:
                return db::ImageType::Other;
            case audio::Image::Type::FileIcon:
                return db::ImageType::FileIcon;
            case audio::Image::Type::OtherFileIcon:
                return db::ImageType::OtherFileIcon;
            case audio::Image::Type::FrontCover:
                return db::ImageType::FrontCover;
            case audio::Image::Type::BackCover:
                return db::ImageType::BackCover;
            case audio::Image::Type::LeafletPage:
                return db::ImageType::LeafletPage;
            case audio::Image::Type::Media:
                return db::ImageType::Media;
            case audio::Image::Type::LeadArtist:
                return db::ImageType::LeadArtist;
            case audio::Image::Type::Artist:
                return db::ImageType::Artist;
            case audio::Image::Type::Conductor:
                return db::ImageType::Conductor;
            case audio::Image::Type::Band:
                return db::ImageType::Band;
            case audio::Image::Type::Composer:
                return db::ImageType::Composer;
            case audio::Image::Type::Lyricist:
                return db::ImageType::Lyricist;
            case audio::Image::Type::RecordingLocation:
                return db::ImageType::RecordingLocation;
            case audio::Image::Type::DuringRecording:
                return db::ImageType::DuringRecording;
            case audio::Image::Type::DuringPerformance:
                return db::ImageType::DuringPerformance;
            case audio::Image::Type::MovieScreenCapture:
                return db::ImageType::MovieScreenCapture;
            case audio::Image::Type::ColouredFish:
                return db::ImageType::ColouredFish;
            case audio::Image::Type::Illustration:
                return db::ImageType::Illustration;
            case audio::Image::Type::BandLogo:
                return db::ImageType::BandLogo;
            case audio::Image::Type::PublisherLogo:
                return db::ImageType::PublisherLogo;
            }

            return db::ImageType::Unknown;
        }

        db::TrackEmbeddedImage::pointer getOrCreateTrackEmbeddedImage(db::Session& session, const ImageInfo& imageInfo)
        {
            db::TrackEmbeddedImage::pointer image{ db::TrackEmbeddedImage::find(session, imageInfo.size, db::ImageHashType{ imageInfo.hash }) };
            if (!image)
            {
                image = session.create<db::TrackEmbeddedImage>();
                image.modify()->setSize(imageInfo.size);
                image.modify()->setHash(db::ImageHashType{ imageInfo.hash });
                image.modify()->setWidth(imageInfo.properties.width);
                image.modify()->setHeight(imageInfo.properties.height);
                image.modify()->setMimeType(imageInfo.mimeType);

                session.create<db::Artwork>(image);
            }

            return image;
        }

        db::TrackEmbeddedImageLink::pointer createTrackEmbeddedImageLink(db::Session& session, const db::Track::pointer& dbTrack, const ImageInfo& imageInfo)
        {
            const db::TrackEmbeddedImage::pointer image{ getOrCreateTrackEmbeddedImage(session, imageInfo) };
            db::TrackEmbeddedImageLink::pointer imageLink{ session.create<db::TrackEmbeddedImageLink>(dbTrack, image) };
            imageLink.modify()->setIndex(imageInfo.index);
            imageLink.modify()->setType(convertImageType(imageInfo.type));
            imageLink.modify()->setDescription(imageInfo.description);

            return imageLink;
        }

        void updateEmbeddedImages(db::Session& session, db::Track::pointer& dbTrack, std::span<const ImageInfo> images)
        {
            dbTrack.modify()->clearEmbeddedImageLinks();
            for (const ImageInfo& imageInfo : images)
            {
                db::TrackEmbeddedImageLink::pointer link{ createTrackEmbeddedImageLink(session, dbTrack, imageInfo) };
                dbTrack.modify()->addEmbeddedImageLink(link);
            }
        }

        db::Advisory getAdvisory(std::optional<Track::Advisory> advisory)
        {
            if (!advisory)
                return db::Advisory::UnSet;

            switch (advisory.value())
            {
            case Track::Advisory::Clean:
                return db::Advisory::Clean;
            case Track::Advisory::Explicit:
                return db::Advisory::Explicit;
            case Track::Advisory::Unknown:
                return db::Advisory::Unknown;
            }

            return db::Advisory::UnSet;
        }

        db::Track::pointer findMovedTrackBySizeAndMetaData(db::Session& session, const Track& parsedTrack, const std::filesystem::path& trackPath, size_t fileSize)
        {
            db::Track::FindParameters params;
            // Add as many fields as possible to limit errors
            params.setName(parsedTrack.title);
            params.setFileSize(fileSize);
            if (parsedTrack.medium)
            {
                if (parsedTrack.medium->release)
                    params.setReleaseName(parsedTrack.medium->release->name);
            }
            if (parsedTrack.position)
                params.setTrackNumber(*parsedTrack.position);

            bool error{};
            db::Track::pointer res;
            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                // Check that the track is truly no longer where it was during the last scan
                std::error_code ec;
                if (std::filesystem::exists(track->getAbsoluteFilePath(), ec))
                    return;

                if (res)
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Found too many candidates for file move. New file = " << trackPath << ", candidate = " << track->getAbsoluteFilePath() << ", previous candidate = " << res->getAbsoluteFilePath());
                    error = true;
                }
                res = track;
            });

            if (error)
                res = db::Track::pointer{};

            return res;
        }

        void fillInArtistsWithMbid(std::span<const Artist> artists, std::unordered_map<std::string_view, core::UUID>& artistsWithMbid)
        {
            for (const Artist& artist : artists)
            {
                if (artist.mbid.has_value())
                {
                    // there may collisions, we don't want to replace
                    artistsWithMbid.emplace(artist.name, *artist.mbid);
                }
            }
        }

        void fillInMbids(std::span<Artist> artists, const std::unordered_map<std::string_view, core::UUID>& artistsWithMbid)
        {
            for (Artist& artist : artists)
            {
                if (!artist.mbid)
                {
                    const auto it{ artistsWithMbid.find(artist.name) };
                    if (it != std::cend(artistsWithMbid))
                        artist.mbid = it->second;
                }
            }
        }

        void fillMissingMbids(Track& track)
        {
            // first pass: collect all artists that have mbids
            std::unordered_map<std::string_view, core::UUID> artistsWithMbid;

            // For now, mbids can only set in artist and album artist tags
            // filling order is important: we estimate track-level artists are more likely
            // to be set in other fields than album artists
            fillInArtistsWithMbid(track.artists, artistsWithMbid);
            if (track.medium && track.medium->release)
                fillInArtistsWithMbid(track.medium->release->artists, artistsWithMbid);

            // second pass: fill in all artists that have no mbid set with the same name
            fillInMbids(track.conductorArtists, artistsWithMbid);
            fillInMbids(track.composerArtists, artistsWithMbid);
            fillInMbids(track.lyricistArtists, artistsWithMbid);
            fillInMbids(track.mixerArtists, artistsWithMbid);
            fillInMbids(track.producerArtists, artistsWithMbid);
            fillInMbids(track.remixerArtists, artistsWithMbid);
            for (auto& [role, artists] : track.performerArtists)
                fillInMbids(artists, artistsWithMbid);
        }
    } // namespace

    AudioFileScanOperation::AudioFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, const TrackMetadataParser& metadataParser, const audio::ParserOptions& parserOptions)
        : FileScanOperationBase{ std::move(fileToScan), db, settings }
        , _metadataParser{ metadataParser }
        , _parserOptions{ parserOptions }
    {
    }

    AudioFileScanOperation::~AudioFileScanOperation() = default;

    void AudioFileScanOperation::scan()
    {
        try
        {
            auto audioFileInfo{ audio::parseAudioFile(getFilePath(), _parserOptions) };

            _file.emplace();

            _file->audioProperties = audioFileInfo->getAudioProperties();
            _file->track = _metadataParser.parseTrackMetaData(audioFileInfo->getTagReader());

            // We fill missing artist mbids with mbids found on other artist roles
            fillMissingMbids(_file->track);

            std::size_t index{};
            audioFileInfo->getImageReader().visitImages([&](const audio::Image& image) {
                try
                {
                    image::ImageProperties properties{ image::probeImage(image.data) };

                    ImageInfo info;
                    info.index = index;
                    info.type = image.type;
                    {
                        LMS_SCOPED_TRACE_DETAILED("Scanner", "ImageHash");
                        info.hash = core::xxHash3_64(image.data);
                    }
                    info.size = image.data.size();
                    info.mimeType = image.mimeType;
                    info.description = image.description;
                    info.properties = properties;

                    _file->images.push_back(std::move(info));
                }
                catch (const image::Exception& e)
                {
                    addError<EmbeddedImageScanError>(getFilePath(), index, e.what());
                }

                index++;
            });
        }
        catch (const audio::AudioFileNoAudioPropertiesException&)
        {
            addError<NoAudioTrackFoundError>(getFilePath());
        }
        catch (const audio::IOException& e)
        {
            addError<IOScanError>(getFilePath(), e.getErrorCode());
        }
        catch (const audio::Exception& e)
        {
            addError<AudioFileScanError>(getFilePath());
        }
    }

    AudioFileScanOperation::OperationResult AudioFileScanOperation::processResult()
    {
        LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessAudioScanData");

        db::Session& dbSession{ getDb().getTLSSession() };
        db::Track::pointer track{ db::Track::findByPath(dbSession, getFilePath()) };
        if (!_file)
        {
            if (track)
            {
                track.remove();
                return OperationResult::Removed;
            }
            return OperationResult::Skipped;
        }

        if (_file->track.mbid && (!track || getScannerSettings().skipDuplicateTrackMBID))
        {
            std::vector<db::Track::pointer> duplicateTracks{ db::Track::findByMBID(dbSession, *_file->track.mbid) };

            // find for an existing track MBID as the file may have just been moved
            if (!track && duplicateTracks.size() == 1)
            {
                db::Track::pointer otherTrack{ duplicateTracks.front() };
                std::error_code ec;
                if (!std::filesystem::exists(otherTrack->getAbsoluteFilePath(), ec))
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Considering track " << getFilePath() << " moved from " << otherTrack->getAbsoluteFilePath());
                    track = otherTrack;
                    track.modify()->setAbsoluteFilePath(getFilePath());
                }
            }

            // Skip duplicate track MBID
            if (getScannerSettings().skipDuplicateTrackMBID)
            {
                for (db::Track::pointer& otherTrack : duplicateTracks)
                {
                    // Skip ourselves
                    if (track && track->getId() == otherTrack->getId())
                        continue;

                    // Skip if duplicate files no longer in media root: as it will be removed later, we will end up with no file
                    auto& mediaLibraries{ getScannerSettings().mediaLibraries };
                    if (std::none_of(std::cbegin(mediaLibraries), std::cend(mediaLibraries),
                                     [&](const MediaLibraryInfo& libraryInfo) {
                                         return core::pathUtils::isPathInRootPath(getFilePath(), libraryInfo.rootDirectory, &excludeDirFileName);
                                     }))
                    {
                        continue;
                    }

                    LMS_LOG(DBUPDATER, DEBUG, "Skipped " << getFilePath() << ": same MBID already found in " << otherTrack->getAbsoluteFilePath());
                    // As this MBID already exists, just remove what we just scanned
                    if (track)
                    {
                        track.remove();

                        LMS_LOG(DBUPDATER, DEBUG, "Removed " << getFilePath() << ": same MBID already found in " << otherTrack->getAbsoluteFilePath());
                        return OperationResult::Removed;
                    }

                    return OperationResult::Skipped;
                }
            }
        }

        if (!track)
        {
            // maybe the file just moved?
            track = findMovedTrackBySizeAndMetaData(dbSession, _file->track, getFilePath(), getFileSize());
            if (track)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Considering track " << getFilePath() << " moved from " << track->getAbsoluteFilePath());
                track.modify()->setAbsoluteFilePath(getFilePath());
            }
        }

        // We estimate this is an audio file if the duration is not null
        if (_file->audioProperties.duration == std::chrono::milliseconds::zero())
        {
            addError<BadAudioDurationError>(getFilePath());

            if (track)
            {
                track.remove();
                return OperationResult::Removed;
            }
            return OperationResult::Skipped;
        }

        // ***** Title
        std::string title;
        if (!_file->track.title.empty())
            title = _file->track.title;
        else
        {
            // TODO parse file name to guess track etc.
            // For now, we just use file name as title
            title = getFilePath().filename().string();
        }

        // If file already exists, update its data
        // Otherwise, create it
        bool added{};
        if (!track)
        {
            track = dbSession.create<db::Track>();
            added = true;

            track.modify()->setAbsoluteFilePath(getFilePath());
            track.modify()->setAddedTime(getMediaLibrary().firstScan ? getLastWriteTime() : Wt::WDateTime::currentDateTime()); // may be erased by encodingTime
        }

        // Track related data
        assert(track);
        track.modify()->setScanVersion(getScannerSettings().audioScanVersion);

        // Audio properties
        track.modify()->setBitrate(_file->audioProperties.bitrate ? *_file->audioProperties.bitrate : 0);
        track.modify()->setBitsPerSample(_file->audioProperties.bitsPerSample ? *_file->audioProperties.bitsPerSample : 0);
        track.modify()->setChannelCount(_file->audioProperties.channelCount ? *_file->audioProperties.channelCount : 0);
        track.modify()->setDuration(_file->audioProperties.duration);
        track.modify()->setSampleRate(_file->audioProperties.sampleRate ? *_file->audioProperties.sampleRate : 0);

        track.modify()->setFileSize(getFileSize());
        track.modify()->setLastWriteTime(getLastWriteTime());

        if (_file->track.encodingTime.isValid())
        {
            const core::PartialDateTime& encodingTime{ _file->track.encodingTime };
            Wt::WDate date;
            Wt::WTime time;
            if (encodingTime.getPrecision() >= core::PartialDateTime::Precision::Day)
                date = Wt::WDate{ *encodingTime.getYear(), *encodingTime.getMonth(), *encodingTime.getDay() };
            if (encodingTime.getPrecision() >= core::PartialDateTime::Precision::Sec)
                time = Wt::WTime{ *encodingTime.getHour(), *encodingTime.getMin(), *encodingTime.getSec() };

            if (date.isValid())
                track.modify()->setAddedTime(time.isValid() ? Wt::WDateTime{ date, time } : Wt::WDateTime{ date });
        }

        db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this
        track.modify()->setMediaLibrary(mediaLibrary);
        db::Directory::pointer directory{ utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary) };
        track.modify()->setDirectory(directory);

        track.modify()->clearArtistLinks();

        const helpers::AllowFallbackOnMBIDEntry allowFallback{ getScannerSettings().allowArtistMBIDFallback };
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Artist, _file->track.artists, allowFallback);
        if (_file->track.medium && _file->track.medium->release)
            createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::ReleaseArtist, _file->track.medium->release->artists, allowFallback);

        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Conductor, _file->track.conductorArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Composer, _file->track.composerArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Lyricist, _file->track.lyricistArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Mixer, _file->track.mixerArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Producer, _file->track.producerArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Remixer, _file->track.remixerArtists, allowFallback);

        for (const auto& [role, performers] : _file->track.performerArtists)
            createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Performer, role, performers, allowFallback);

        // For now, alway tie a medium to a release, and a release mst have at least one medium, even if no disc number is set
        if (_file->track.medium && _file->track.medium->release)
        {
            db::Release::pointer release{ getOrCreateRelease(dbSession, *_file->track.medium->release, directory) };
            assert(release);
            track.modify()->setRelease(release);
            track.modify()->setMedium(getOrCreateMedium(dbSession, *_file->track.medium, release));
        }
        else
        {
            track.modify()->setRelease({});
            track.modify()->setMedium({});
        }
        track.modify()->setClusters(getOrCreateClusters(dbSession, _file->track));
        track.modify()->setName(title);
        track.modify()->setTrackNumber(_file->track.position);
        track.modify()->setDate(_file->track.date);
        track.modify()->setOriginalDate(_file->track.originalDate);
        if (!track->getOriginalDate().isValid() && _file->track.originalYear)
            track.modify()->setOriginalDate(core::PartialDateTime{ *_file->track.originalYear });

        // If a file has an OriginalDate but no date, set it to ease filtering
        if (!_file->track.date.isValid() && _file->track.originalDate.isValid())
            track.modify()->setDate(_file->track.originalDate);

        track.modify()->setRecordingMBID(_file->track.recordingMBID);
        track.modify()->setTrackMBID(_file->track.mbid);
        if (auto trackFeatures{ db::TrackFeatures::find(dbSession, track->getId()) })
            trackFeatures.remove(); // TODO: only if MBID changed?
        track.modify()->setCopyright(_file->track.copyright);
        track.modify()->setCopyrightURL(_file->track.copyrightURL);
        track.modify()->setAdvisory(getAdvisory(_file->track.advisory));
        track.modify()->setComment(!_file->track.comments.empty() ? _file->track.comments.front() : ""); // only take the first one for now
        track.modify()->setReplayGain(_file->track.replayGain);
        track.modify()->setArtistDisplayName(_file->track.artistDisplayName);

        track.modify()->clearEmbeddedLyrics();
        for (const Lyrics& lyricsInfo : _file->track.lyrics)
            track.modify()->addLyrics(createLyrics(dbSession, lyricsInfo));

        updateEmbeddedImages(dbSession, track, _file->images);

        if (added)
        {
            LMS_LOG(DBUPDATER, DEBUG, "Added audio file " << getFilePath());
            return OperationResult::Added;
        }

        LMS_LOG(DBUPDATER, DEBUG, "Updated audio file " << getFilePath());
        return OperationResult::Updated;
    }
} // namespace lms::scanner
