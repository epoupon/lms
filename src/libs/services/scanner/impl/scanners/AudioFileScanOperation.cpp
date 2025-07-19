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
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"
#include "metadata/Exception.hpp"
#include "metadata/IAudioFileParser.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "IFileScanOperation.hpp"
#include "ScannerSettings.hpp"
#include "Utils.hpp"
#include "helpers/ArtistHelpers.hpp"

namespace lms::scanner
{
    namespace
    {
        void createTrackArtistLinks(db::Session& session, const db::Track::pointer& track, db::TrackArtistLinkType linkType, std::string_view role, std::span<const metadata::Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
        {
            for (const metadata::Artist& artistInfo : artists)
            {
                db::Artist::pointer artist{ helpers::getOrCreateArtist(session, artistInfo, allowArtistMBIDFallback) };

                const bool matchedUsingMbid{ artistInfo.mbid.has_value() && artist->getMBID() == artistInfo.mbid };
                db::TrackArtistLink::pointer link{ session.create<db::TrackArtistLink>(track, artist, linkType, role, matchedUsingMbid) };
                link.modify()->setArtistName(artistInfo.name);
                if (artistInfo.sortName)
                    link.modify()->setArtistSortName(*artistInfo.sortName);
            }
        }

        void createTrackArtistLinks(db::Session& session, const db::Track::pointer& track, db::TrackArtistLinkType linkType, std::span<const metadata::Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
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
            if (release->getComment() != releaseInfo.comment)
                release.modify()->setComment(releaseInfo.comment);
            if (release->getReleaseTypeNames() != releaseInfo.releaseTypes)
            {
                release.modify()->clearReleaseTypes();
                for (std::string_view releaseType : releaseInfo.releaseTypes)
                    release.modify()->addReleaseType(getOrCreateReleaseType(session, releaseType));
            }
            if (release->getCountryNames() != releaseInfo.countries)
            {
                release.modify()->clearCountries();
                for (std::string_view country : releaseInfo.countries)
                    release.modify()->addCountry(getOrCreateCountry(session, country));
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

        db::ImageType convertImageType(metadata::Image::Type type)
        {
            switch (type)
            {
            case metadata::Image::Type::Unknown:
                return db::ImageType::Unknown;
            case metadata::Image::Type::Other:
                return db::ImageType::Other;
            case metadata::Image::Type::FileIcon:
                return db::ImageType::FileIcon;
            case metadata::Image::Type::OtherFileIcon:
                return db::ImageType::OtherFileIcon;
            case metadata::Image::Type::FrontCover:
                return db::ImageType::FrontCover;
            case metadata::Image::Type::BackCover:
                return db::ImageType::BackCover;
            case metadata::Image::Type::LeafletPage:
                return db::ImageType::LeafletPage;
            case metadata::Image::Type::Media:
                return db::ImageType::Media;
            case metadata::Image::Type::LeadArtist:
                return db::ImageType::LeadArtist;
            case metadata::Image::Type::Artist:
                return db::ImageType::Artist;
            case metadata::Image::Type::Conductor:
                return db::ImageType::Conductor;
            case metadata::Image::Type::Band:
                return db::ImageType::Band;
            case metadata::Image::Type::Composer:
                return db::ImageType::Composer;
            case metadata::Image::Type::Lyricist:
                return db::ImageType::Lyricist;
            case metadata::Image::Type::RecordingLocation:
                return db::ImageType::RecordingLocation;
            case metadata::Image::Type::DuringRecording:
                return db::ImageType::DuringRecording;
            case metadata::Image::Type::DuringPerformance:
                return db::ImageType::DuringPerformance;
            case metadata::Image::Type::MovieScreenCapture:
                return db::ImageType::MovieScreenCapture;
            case metadata::Image::Type::ColouredFish:
                return db::ImageType::ColouredFish;
            case metadata::Image::Type::Illustration:
                return db::ImageType::Illustration;
            case metadata::Image::Type::BandLogo:
                return db::ImageType::BandLogo;
            case metadata::Image::Type::PublisherLogo:
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

        db::TrackEmbeddedImageLink::pointer createTrackEmbeddedImageLink(db::Session& session, const db::Track::pointer& track, const ImageInfo& imageInfo)
        {
            const db::TrackEmbeddedImage::pointer image{ getOrCreateTrackEmbeddedImage(session, imageInfo) };
            db::TrackEmbeddedImageLink::pointer imageLink{ session.create<db::TrackEmbeddedImageLink>(track, image) };
            imageLink.modify()->setIndex(imageInfo.index);
            imageLink.modify()->setType(convertImageType(imageInfo.type));
            imageLink.modify()->setDescription(imageInfo.description);

            return imageLink;
        }

        void updateEmbeddedImages(db::Session& session, db::Track::pointer& track, std::span<const ImageInfo> images)
        {
            track.modify()->clearEmbeddedImageLinks();
            for (const ImageInfo& imageInfo : images)
            {
                db::TrackEmbeddedImageLink::pointer link{ createTrackEmbeddedImageLink(session, track, imageInfo) };
                track.modify()->addEmbeddedImageLink(link);
            }
        }

        db::Advisory getAdvisory(std::optional<metadata::Track::Advisory> advisory)
        {
            if (!advisory)
                return db::Advisory::UnSet;

            switch (advisory.value())
            {
            case metadata::Track::Advisory::Clean:
                return db::Advisory::Clean;
            case metadata::Track::Advisory::Explicit:
                return db::Advisory::Explicit;
            case metadata::Track::Advisory::Unknown:
                return db::Advisory::Unknown;
            }

            return db::Advisory::UnSet;
        }

        db::Track::pointer findMovedTrackBySizeAndMetaData(db::Session& session, const metadata::Track& parsedTrack, const std::filesystem::path& trackPath, size_t fileSize)
        {
            db::Track::FindParameters params;
            // Add as many fields as possible to limit errors
            params.setName(parsedTrack.title);
            params.setFileSize(fileSize);
            if (parsedTrack.medium)
            {
                if (parsedTrack.medium->position)
                    params.setDiscNumber(*parsedTrack.medium->position);
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

        void fillInArtistsWithMbid(std::span<const metadata::Artist> artists, std::unordered_map<std::string_view, core::UUID>& artistsWithMbid)
        {
            for (const metadata::Artist& artist : artists)
            {
                if (artist.mbid.has_value())
                {
                    // there may collisions, we don't want to replace
                    artistsWithMbid.emplace(artist.name, *artist.mbid);
                }
            }
        }

        void fillInMbids(std::span<metadata::Artist> artists, const std::unordered_map<std::string_view, core::UUID>& artistsWithMbid)
        {
            for (metadata::Artist& artist : artists)
            {
                if (!artist.mbid)
                {
                    const auto it{ artistsWithMbid.find(artist.name) };
                    if (it != std::cend(artistsWithMbid))
                        artist.mbid = it->second;
                }
            }
        }

        void fillMissingMbids(metadata::Track& track)
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

    AudioFileScanOperation::AudioFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, metadata::IAudioFileParser& parser)
        : FileScanOperationBase{ std::move(fileToScan), db, settings }
        , _parser{ parser }
    {
    }

    AudioFileScanOperation::~AudioFileScanOperation() = default;

    void AudioFileScanOperation::scan()
    {
        std::unique_ptr<metadata::Track> track;

        try
        {
            _parsedTrack = _parser.parseMetaData(getFilePath());

            // We fill missing artist mbids with mbids found on other artist roles
            fillMissingMbids(*_parsedTrack);

            std::size_t index{};
            _parser.parseImages(getFilePath(), [&](const metadata::Image& image) {
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

                    _parsedImages.push_back(std::move(info));
                }
                catch (const image::Exception& e)
                {
                    addError<EmbeddedImageScanError>(getFilePath(), index);
                }

                index++;
            });
        }
        catch (const metadata::AudioFileNoAudioPropertiesException&)
        {
            addError<NoAudioTrackFoundError>(getFilePath());
        }
        catch (const metadata::IOException& e)
        {
            addError<IOScanError>(getFilePath(), e.getErrorCode());
        }
        catch (const metadata::Exception& e)
        {
            addError<AudioFileScanError>(getFilePath());
        }
    }

    AudioFileScanOperation::OperationResult AudioFileScanOperation::processResult()
    {
        LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessAudioScanData");

        db::Session& dbSession{ getDb().getTLSSession() };
        db::Track::pointer track{ db::Track::findByPath(dbSession, getFilePath()) };
        if (!_parsedTrack)
        {
            if (track)
            {
                track.remove();
                return OperationResult::Removed;
            }
            return OperationResult::Skipped;
        }

        if (_parsedTrack->mbid && (!track || getScannerSettings().skipDuplicateTrackMBID))
        {
            std::vector<db::Track::pointer> duplicateTracks{ db::Track::findByMBID(dbSession, *_parsedTrack->mbid) };

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
            track = findMovedTrackBySizeAndMetaData(dbSession, *_parsedTrack, getFilePath(), getFileSize());
            if (track)
            {
                LMS_LOG(DBUPDATER, DEBUG, "Considering track " << getFilePath() << " moved from " << track->getAbsoluteFilePath());
                track.modify()->setAbsoluteFilePath(getFilePath());
            }
        }

        // We estimate this is an audio file if the duration is not null
        if (_parsedTrack->audioProperties.duration == std::chrono::milliseconds::zero())
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
        if (!_parsedTrack->title.empty())
            title = _parsedTrack->title;
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
        track.modify()->setBitrate(_parsedTrack->audioProperties.bitrate);
        track.modify()->setBitsPerSample(_parsedTrack->audioProperties.bitsPerSample);
        track.modify()->setChannelCount(_parsedTrack->audioProperties.channelCount);
        track.modify()->setDuration(_parsedTrack->audioProperties.duration);
        track.modify()->setSampleRate(_parsedTrack->audioProperties.sampleRate);

        track.modify()->setFileSize(getFileSize());
        track.modify()->setLastWriteTime(getLastWriteTime());

        if (_parsedTrack->encodingTime.isValid())
        {
            const core::PartialDateTime& encodingTime{ _parsedTrack->encodingTime };
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
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Artist, _parsedTrack->artists, allowFallback);
        if (_parsedTrack->medium && _parsedTrack->medium->release)
            createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::ReleaseArtist, _parsedTrack->medium->release->artists, allowFallback);

        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Conductor, _parsedTrack->conductorArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Composer, _parsedTrack->composerArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Lyricist, _parsedTrack->lyricistArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Mixer, _parsedTrack->mixerArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Producer, _parsedTrack->producerArtists, allowFallback);
        createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Remixer, _parsedTrack->remixerArtists, allowFallback);

        for (const auto& [role, performers] : _parsedTrack->performerArtists)
            createTrackArtistLinks(dbSession, track, db::TrackArtistLinkType::Performer, role, performers, allowFallback);

        if (_parsedTrack->medium && _parsedTrack->medium->release)
            track.modify()->setRelease(getOrCreateRelease(dbSession, *_parsedTrack->medium->release, directory));
        else
            track.modify()->setRelease({});
        track.modify()->setTotalTrack(_parsedTrack->medium ? _parsedTrack->medium->trackCount : std::nullopt);
        track.modify()->setReleaseReplayGain(_parsedTrack->medium ? _parsedTrack->medium->replayGain : std::nullopt);
        track.modify()->setDiscSubtitle(_parsedTrack->medium ? _parsedTrack->medium->name : "");
        track.modify()->setClusters(getOrCreateClusters(dbSession, *_parsedTrack));
        track.modify()->setName(title);
        track.modify()->setTrackNumber(_parsedTrack->position);
        track.modify()->setDiscNumber(_parsedTrack->medium ? _parsedTrack->medium->position : std::nullopt);
        track.modify()->setDate(_parsedTrack->date);
        track.modify()->setOriginalDate(_parsedTrack->originalDate);
        if (!track->getOriginalDate().isValid() && _parsedTrack->originalYear)
            track.modify()->setOriginalDate(core::PartialDateTime{ *_parsedTrack->originalYear });

        // If a file has an OriginalDate but no date, set it to ease filtering
        if (!_parsedTrack->date.isValid() && _parsedTrack->originalDate.isValid())
            track.modify()->setDate(_parsedTrack->originalDate);

        track.modify()->setRecordingMBID(_parsedTrack->recordingMBID);
        track.modify()->setTrackMBID(_parsedTrack->mbid);
        if (auto trackFeatures{ db::TrackFeatures::find(dbSession, track->getId()) })
            trackFeatures.remove(); // TODO: only if MBID changed?
        track.modify()->setCopyright(_parsedTrack->copyright);
        track.modify()->setCopyrightURL(_parsedTrack->copyrightURL);
        track.modify()->setAdvisory(getAdvisory(_parsedTrack->advisory));
        track.modify()->setComment(!_parsedTrack->comments.empty() ? _parsedTrack->comments.front() : ""); // only take the first one for now
        track.modify()->setTrackReplayGain(_parsedTrack->replayGain);
        track.modify()->setArtistDisplayName(_parsedTrack->artistDisplayName);

        track.modify()->clearEmbeddedLyrics();
        for (const metadata::Lyrics& lyricsInfo : _parsedTrack->lyrics)
            track.modify()->addLyrics(createLyrics(dbSession, lyricsInfo));

        updateEmbeddedImages(dbSession, track, _parsedImages);

        if (added)
        {
            LMS_LOG(DBUPDATER, DEBUG, "Added audio file " << getFilePath());
            return OperationResult::Added;
        }

        LMS_LOG(DBUPDATER, DEBUG, "Updated audio file " << getFilePath());
        return OperationResult::Updated;
    }
} // namespace lms::scanner
