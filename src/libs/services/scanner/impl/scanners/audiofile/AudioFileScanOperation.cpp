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

#include <array>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/PartialDateTime.hpp"
#include "core/Path.hpp"
#include "core/XxHash3.hpp"

#include "audio/Exception.hpp"
#include "audio/IAudioFileInfo.hpp"
#include "audio/IAudioFileInfoParser.hpp"
#include "audio/IImageReader.hpp"
#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "database/objects/Work.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"

#include "services/scanner/ScanErrors.hpp"

#include "IgnoreRules.hpp"
#include "ScannerSettings.hpp"
#include "helpers/ArtistHelpers.hpp"
#include "scanners/IFileScanOperation.hpp"
#include "scanners/Utils.hpp"
#include "scanners/audiofile/TrackMetadataParser.hpp"
#include "scanners/audiofile/Utils.hpp"

namespace lms::scanner
{
    namespace
    {
        void createTrackArtistLinks(db::ISession& session, db::TrackId track, db::TrackArtistLinkType linkType, std::string_view role, std::span<const Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
        {
            for (const Artist& artist : artists)
            {
                const db::Artist dbArtist{ helpers::getOrCreateArtist(session, artist, allowArtistMBIDFallback) };

                const bool matchedUsingMbid{ artist.mbid.has_value() && dbArtist.getMBID() == artist.mbid };
                db::TrackArtistLink link{ db::TrackArtistLink::create(session, track, dbArtist.getId(), linkType, role, matchedUsingMbid) };
                link.setArtistName(artist.name);
                if (artist.sortName)
                    link.setArtistSortName(*artist.sortName);
                db::TrackArtistLink::update(session, link);
            }
        }

        void createTrackArtistLinks(db::ISession& session, db::TrackId track, db::TrackArtistLinkType linkType, std::span<const Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
        {
            constexpr std::string_view noRole{};
            createTrackArtistLinks(session, track, linkType, noRole, artists, allowArtistMBIDFallback);
        }

        void createReleaseArtistLinks(db::ISession& session, db::ReleaseId releaseId, std::span<const Artist> artists, helpers::AllowFallbackOnMBIDEntry allowArtistMBIDFallback)
        {
            for (const Artist& artist : artists)
            {
                const db::Artist dbArtist{ helpers::getOrCreateArtist(session, artist, allowArtistMBIDFallback) };

                const bool matchedUsingMbid{ artist.mbid.has_value() && dbArtist.getMBID() == artist.mbid };
                db::ReleaseArtistLink link{ db::ReleaseArtistLink::create(session, releaseId, dbArtist.getId(), matchedUsingMbid) };
                link.setArtistName(artist.name);
                if (artist.sortName)
                    link.setArtistSortName(*artist.sortName);
                db::ReleaseArtistLink::update(session, link);
            }
        }

        db::ReleaseType getOrCreateReleaseType(db::ISession& session, std::string_view name)
        {
            if (std::optional<db::ReleaseType> releaseType{ db::ReleaseType::find(session, name) })
                return *releaseType;

            return db::ReleaseType::create(session, name);
        }

        db::Country getOrCreateCountry(db::ISession& session, std::string_view name)
        {
            if (std::optional<db::Country> country{ db::Country::find(session, name) })
                return *country;

            return db::Country::create(session, name);
        }

        db::Label getOrCreateLabel(db::ISession& session, std::string_view name)
        {
            if (std::optional<db::Label> label{ db::Label::find(session, name) })
                return *label;

            return db::Label::create(session, name);
        }

        std::vector<std::string> getLabelNames(db::ISession& session, db::ReleaseId releaseId)
        {
            std::vector<std::string> names;
            db::Label::find(session, releaseId, [&](const db::Label& label) {
                names.emplace_back(label.getName());
            });

            return names;
        }

        std::vector<std::string> getCountryNames(db::ISession& session, db::ReleaseId releaseId)
        {
            std::vector<std::string> names;
            db::Country::find(session, releaseId, [&](const db::Country& country) {
                names.emplace_back(country.getName());
            });

            return names;
        }

        std::vector<std::string> getReleaseTypeNames(db::ISession& session, db::ReleaseId releaseId)
        {
            std::vector<std::string> names;
            db::ReleaseType::find(session, releaseId, [&](const db::ReleaseType& releaseType) {
                names.emplace_back(releaseType.getName());
            });

            return names;
        }

        bool needUpdateReleaseArtists(db::ISession& session, db::ReleaseId releaseId, const Release& release)
        {
            std::vector<db::ReleaseArtistLink> dbArtistLinks;
            db::ReleaseArtistLink::find(session, releaseId, [&](const db::ReleaseArtistLink& link) {
                dbArtistLinks.push_back(link);
            });

            if (dbArtistLinks.size() != release.artists.size())
                return true;

            // Must be in same order
            for (std::size_t i{}; i < dbArtistLinks.size(); ++i)
            {
                const db::ReleaseArtistLink& dbArtistLink{ dbArtistLinks[i] };
                const Artist& artist{ release.artists[i] };

                if (dbArtistLink.getArtistName() != artist.name)
                    return true;

                const std::string_view artistSortName{ artist.sortName ? std::string_view{ *artist.sortName } : std::string_view{} };
                if (dbArtistLink.getArtistSortName() != artistSortName)
                    return true;

                if (!dbArtistLink.isArtistMBIDMatched() && artist.mbid)
                    return true;

                if (dbArtistLink.isArtistMBIDMatched() && !artist.mbid)
                    return true;

                if (artist.mbid)
                {
                    const std::optional<db::Artist> dbArtist{ db::Artist::find(session, dbArtistLink.getArtistId()) };
                    if (!dbArtist)
                        return true;

                    if (dbArtist->getMBID() != artist.mbid)
                        return true;
                }
            }

            return false;
        }

        void updateReleaseIfNeeded(db::ISession& session, db::Release dbRelease, const Release& release, helpers::AllowFallbackOnMBIDEntry allowFallback)
        {
            bool changed{};
            if (dbRelease.getName() != release.name)
            {
                dbRelease.setName(release.name);
                changed = true;
            }
            if (dbRelease.getSortName() != release.sortName)
            {
                dbRelease.setSortName(release.sortName);
                changed = true;
            }
            if (dbRelease.getGroupMBID() != release.groupMBID)
            {
                dbRelease.setGroupMBID(release.groupMBID);
                changed = true;
            }
            if (dbRelease.getTotalDisc() != release.mediumCount)
            {
                dbRelease.setTotalDisc(release.mediumCount);
                changed = true;
            }
            if (dbRelease.getArtistDisplayName() != release.artistDisplayName)
            {
                dbRelease.setArtistDisplayName(release.artistDisplayName);
                changed = true;
            }
            if (dbRelease.isCompilation() != release.isCompilation)
            {
                dbRelease.setCompilation(release.isCompilation);
                changed = true;
            }
            if (dbRelease.getBarcode() != release.barcode)
            {
                dbRelease.setBarcode(release.barcode);
                changed = true;
            }
            if (dbRelease.getComment() != release.comment)
            {
                dbRelease.setComment(release.comment);
                changed = true;
            }
            if (changed)
                db::Release::update(session, dbRelease);

            const db::ReleaseId releaseId{ dbRelease.getId() };
            if (getReleaseTypeNames(session, releaseId) != release.releaseTypes)
            {
                db::Release::clearReleaseTypes(session, releaseId);
                for (std::string_view releaseType : release.releaseTypes)
                    db::Release::addReleaseType(session, releaseId, getOrCreateReleaseType(session, releaseType).getId());
            }
            if (getCountryNames(session, releaseId) != release.countries)
            {
                db::Release::clearCountries(session, releaseId);
                for (std::string_view country : release.countries)
                    db::Release::addCountry(session, releaseId, getOrCreateCountry(session, country).getId());
            }
            if (getLabelNames(session, releaseId) != release.labels)
            {
                db::Release::clearLabels(session, releaseId);
                for (std::string_view label : release.labels)
                    db::Release::addLabel(session, releaseId, getOrCreateLabel(session, label).getId());
            }
            if (needUpdateReleaseArtists(session, releaseId, release))
            {
                db::Release::clearArtistLinks(session, releaseId);
                createReleaseArtistLinks(session, releaseId, release.artists, allowFallback);
            }
        }

        // Compare release level info
        bool isReleaseMatching(db::ISession& session, const db::Release& dbCandidateRelease, const Release& release)
        {
            // TODO: add more criterias?
            return dbCandidateRelease.getName() == release.name
                && dbCandidateRelease.getTotalDisc() == release.mediumCount
                && dbCandidateRelease.isCompilation() == release.isCompilation
                && getLabelNames(session, dbCandidateRelease.getId()) == release.labels
                && dbCandidateRelease.getBarcode() == release.barcode;
        }

        db::ReleaseId getOrCreateRelease(db::ISession& session, const Release& release, const db::Directory& currentDirectory, helpers::AllowFallbackOnMBIDEntry allowFallback)
        {
            std::optional<db::Release> dbRelease;

            // First try to get by MBID: fastest, safest
            if (release.mbid)
            {
                dbRelease = db::Release::find(session, *release.mbid);
                if (!dbRelease)
                    dbRelease = db::Release::create(session, release.name, release.mbid);
            }
            else if (release.name.empty())
            {
                // No release name (only mbid) -> nothing to do
                return db::ReleaseId{};
            }

            // Fall back on release name (collisions may occur)
            // First try using all sibling directories (case for Album/DiscX), only if the disc number is set
            const db::DirectoryId parentDirectoryId{ currentDirectory.getParentDirectoryId() };
            if (!dbRelease && release.mediumCount && *release.mediumCount > 1 && parentDirectoryId.isValid())
            {
                db::Release::FindParameters params;
                params.setParentDirectory(parentDirectoryId);
                params.setName(release.name);
                db::Release::find(session, params, [&](const db::Release& dbCandidateRelease) {
                    // Already found a candidate
                    if (dbRelease)
                        return;

                    // Do not fallback on properly tagged releases
                    if (dbCandidateRelease.getMBID().has_value())
                        return;

                    if (!isReleaseMatching(session, dbCandidateRelease, release))
                        return;

                    dbRelease = dbCandidateRelease;
                });
            }

            // Lastly try in the current directory: we do this at last to have
            // opportunities to merge releases in case of migration / rescan
            if (!dbRelease)
            {
                db::Release::FindParameters params;
                params.setDirectory(currentDirectory.getId());
                params.setName(release.name);
                db::Release::find(session, params, [&](const db::Release& dbCandidateRelease) {
                    // Already found a candidate
                    if (dbRelease)
                        return;

                    // Do not fallback on properly tagged releases
                    if (dbCandidateRelease.getMBID().has_value())
                        return;

                    if (!isReleaseMatching(session, dbCandidateRelease, release))
                        return;

                    dbRelease = dbCandidateRelease;
                });
            }

            if (!dbRelease)
                dbRelease = db::Release::create(session, release.name);

            updateReleaseIfNeeded(session, *dbRelease, release, allowFallback);
            return dbRelease->getId();
        }

        db::MediumId getOrCreateMedium(db::ISession& session, const Medium& medium, db::ReleaseId releaseId)
        {
            std::optional<db::Medium> dbMedium{ db::Medium::find(session, releaseId, medium.position) };
            if (!dbMedium)
                dbMedium = db::Medium::create(session, releaseId);

            bool changed{};
            if (dbMedium->getPosition() != medium.position)
            {
                dbMedium->setPosition(medium.position);
                changed = true;
            }
            if (dbMedium->getMedia() != medium.media)
            {
                dbMedium->setMedia(medium.media);
                changed = true;
            }
            if (dbMedium->getName() != medium.name)
            {
                dbMedium->setName(medium.name);
                changed = true;
            }
            if (dbMedium->getTrackCount() != medium.trackCount)
            {
                dbMedium->setTrackCount(medium.trackCount);
                changed = true;
            }
            if (dbMedium->getReplayGain() != medium.replayGain)
            {
                dbMedium->setReplayGain(medium.replayGain);
                changed = true;
            }

            if (changed)
                db::Medium::update(session, *dbMedium);

            return dbMedium->getId();
        }

        std::vector<db::GenreId> getOrCreateGenres(db::ISession& session, std::span<const std::string> names)
        {
            std::vector<db::GenreId> genres;
            genres.reserve(names.size());
            for (const std::string& name : names)
            {
                std::optional<db::Genre> genre{ db::Genre::find(session, name) };
                if (!genre)
                    genre = db::Genre::create(session, name);
                genres.push_back(genre->getId());
            }
            return genres;
        }

        std::vector<db::GroupingId> getOrCreateGroupings(db::ISession& session, std::span<const std::string> names)
        {
            std::vector<db::GroupingId> groupings;
            groupings.reserve(names.size());
            for (const std::string& name : names)
            {
                std::optional<db::Grouping> grouping{ db::Grouping::find(session, name) };
                if (!grouping)
                    grouping = db::Grouping::create(session, name);
                groupings.push_back(grouping->getId());
            }
            return groupings;
        }

        std::vector<db::LanguageId> getOrCreateLanguages(db::ISession& session, std::span<const std::string> names)
        {
            std::vector<db::LanguageId> languages;
            languages.reserve(names.size());
            for (const std::string& name : names)
            {
                std::optional<db::Language> language{ db::Language::find(session, name) };
                if (!language)
                    language = db::Language::create(session, name);
                languages.push_back(language->getId());
            }
            return languages;
        }

        std::vector<db::MoodId> getOrCreateMoods(db::ISession& session, std::span<const std::string> names)
        {
            std::vector<db::MoodId> moods;
            moods.reserve(names.size());
            for (const std::string& name : names)
            {
                std::optional<db::Mood> mood{ db::Mood::find(session, name) };
                if (!mood)
                    mood = db::Mood::create(session, name);
                moods.push_back(mood->getId());
            }
            return moods;
        }

        std::vector<db::WorkId> getOrCreateWorks(db::ISession& session, db::ReleaseId releaseId, std::span<const Work> works)
        {
            std::vector<db::WorkId> dbWorks;
            dbWorks.reserve(works.size());
            for (const Work& work : works)
            {
                // Work titles are often generic and collide across unrelated works, so
                // without an mbid we only ever match a work already used on the same release, not globally by name
                std::optional<db::Work> dbWork{ work.mbid ? db::Work::find(session, *work.mbid) : (releaseId.isValid() ? db::Work::find(session, releaseId, work.name) : std::optional<db::Work>{}) };
                if (!dbWork)
                    dbWork = db::Work::create(session, work.name, work.mbid);
                else if (dbWork->getName() != work.name)
                {
                    dbWork->setName(work.name);
                    db::Work::update(session, *dbWork);
                }
                dbWorks.push_back(dbWork->getId());
            }
            return dbWorks;
        }

        std::vector<db::ClusterId> getOrCreateClusters(db::ISession& session, const Track& track)
        {
            std::vector<db::ClusterId> clusters;

            auto getOrCreateClusters{ [&](std::string_view tag, std::span<const std::string> values) {
                auto clusterType = db::ClusterType::find(session, tag);
                if (!clusterType)
                    clusterType = db::ClusterType::create(session, tag);

                for (const auto& value : values)
                {
                    auto cluster{ db::Cluster::find(session, clusterType->getId(), value) };
                    if (!cluster)
                        cluster = db::Cluster::create(session, clusterType->getId(), value);

                    clusters.push_back(cluster->getId());
                }
            } };

            for (const auto& [tag, values] : track.userExtraTags)
                getOrCreateClusters(tag, values);

            return clusters;
        }

        void createLyrics(db::ISession& session, db::TrackId trackId, const Lyrics& lyrics)
        {
            db::TrackLyrics dbLyrics{ db::TrackLyrics::create(session) };

            dbLyrics.setTrack(trackId);
            dbLyrics.setLanguage(!lyrics.language.empty() ? lyrics.language : "xxx");
            dbLyrics.setOffset(lyrics.offset);
            dbLyrics.setDisplayArtist(lyrics.displayArtist);
            dbLyrics.setDisplayTitle(lyrics.displayTitle);
            if (!lyrics.synchronizedLines.empty())
                dbLyrics.setSynchronizedLines(lyrics.synchronizedLines);
            else
                dbLyrics.setUnsynchronizedLines(lyrics.unsynchronizedLines);
            db::TrackLyrics::update(session, dbLyrics);
        }

        db::TrackEmbeddedImage getOrCreateTrackEmbeddedImage(db::ISession& session, const ImageInfo& imageInfo)
        {
            if (std::optional<db::TrackEmbeddedImage> image{ db::TrackEmbeddedImage::find(session, imageInfo.size, db::ImageHashType{ imageInfo.hash }) })
                return *image;

            db::TrackEmbeddedImage image{ db::TrackEmbeddedImage::create(session) };
            image.setSize(imageInfo.size);
            image.setHash(db::ImageHashType{ imageInfo.hash });
            image.setWidth(imageInfo.dimensions.width);
            image.setHeight(imageInfo.dimensions.height);
            image.setFormat(imageInfo.format);
            db::TrackEmbeddedImage::update(session, image);

            db::Artwork::create(session, image.getId());

            return image;
        }

        void createTrackEmbeddedImageLink(db::ISession& session, db::TrackId trackId, const ImageInfo& imageInfo)
        {
            const db::TrackEmbeddedImage image{ getOrCreateTrackEmbeddedImage(session, imageInfo) };
            db::TrackEmbeddedImageLink imageLink{ db::TrackEmbeddedImageLink::create(session, trackId, image.getId()) };
            imageLink.setIndex(imageInfo.index);
            imageLink.setType(imageInfo.type);
            imageLink.setDescription(imageInfo.description);
            db::TrackEmbeddedImageLink::update(session, imageLink);
        }

        void updateEmbeddedImages(db::ISession& session, db::TrackId trackId, std::span<const ImageInfo> images)
        {
            db::Track::clearEmbeddedImageLinks(session, trackId);
            for (const ImageInfo& imageInfo : images)
                createTrackEmbeddedImageLink(session, trackId, imageInfo);
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

        std::optional<db::Track> findMovedTrackBySizeAndMetaData(db::ISession& session, const Track& parsedTrack, const std::filesystem::path& trackPath, size_t fileSize)
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
            std::optional<db::Track> res;
            db::Track::find(session, params, [&](const db::Track& track) {
                // Check that the track is truly no longer where it was during the last scan
                std::error_code ec;
                if (std::filesystem::exists(track.getAbsoluteFilePath(), ec))
                    return;

                if (res)
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Found too many candidates for file move. New file = " << trackPath << ", candidate = " << track.getAbsoluteFilePath() << ", previous candidate = " << res->getAbsoluteFilePath());
                    error = true;
                }
                res = track;
            });

            if (error)
                res.reset();

            return res;
        }

    } // namespace

    AudioFileScanOperation::AudioFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, const audio::IAudioFileInfoParser& parser, audio::AudioFileInfoParseOptions::AudioPropertiesReadStyle audioPropertiesReadStyle, const TrackMetadataParser& metadataParser)
        : FileScanOperationBase{ std::move(fileToScan), db, settings }
        , _parser{ parser }
        , _audioPropertiesReadStyle{ audioPropertiesReadStyle }
        , _metadataParser{ metadataParser }
    {
    }

    AudioFileScanOperation::~AudioFileScanOperation() = default;

    void AudioFileScanOperation::scan()
    {
        try
        {
            audio::AudioFileInfoParseOptions options;
            options.audioPropertiesReadStyle = _audioPropertiesReadStyle;
            options.readImages = true;
            options.readTags = true;

            const auto audioFileInfo{ _parser.parse(getFilePath(), options) };

            if (!audioFileInfo->getAudioProperties())
            {
                addError<NoAudioTrackFoundError>(getFilePath());
                return;
            }

            _file.emplace();
            _file->audioProperties = *audioFileInfo->getAudioProperties();
            _file->track = _metadataParser.parseTrackMetaData(*audioFileInfo->getTagReader());

            // We fill missing artist mbids/sort names with the ones found on other artist roles
            utils::fillMissingArtistMetadata(_file->track);

            std::size_t index{};
            audioFileInfo->getImageReader()->visitImages([&](const audio::Image& image) {
                try
                {
                    const image::ImageProperties probed{ image::probeImage(image.data) };

                    ImageInfo info;
                    info.index = index;
                    info.type = image.type;
                    {
                        LMS_SCOPED_TRACE_DETAILED("Scanner", "ImageHash");
                        info.hash = core::XxHash3_64::hash(image.data);
                    }
                    info.size = image.data.size();
                    info.format = probed.format;
                    info.description = image.description;
                    info.dimensions = probed.dimensions.value_or(image::ImageDimensions{});

                    _file->images.push_back(std::move(info));
                }
                catch (const image::Exception& e)
                {
                    addError<EmbeddedImageScanError>(getFilePath(), index, e.what());
                }

                index++;

                return core::Continue;
            });
        }
        catch (const audio::IOFileException& e)
        {
            addError<IOScanError>(e.getPath(), e.getErrorCode());
        }
        catch (const audio::Exception& e)
        {
            addError<AudioFileScanError>(getFilePath());
        }
    }

    // Returns true if any value actually changed.
    bool updateAudioProperties(db::Track& track, const audio::AudioProperties& props)
    {
        const bool changed{ track.getDuration() != props.duration
                            || track.getContainer() != props.container
                            || track.getCodec() != props.codec
                            || track.getBitrate() != props.bitrate
                            || track.getChannelCount() != props.channelCount
                            || track.getSampleRate() != props.sampleRate
                            || track.getBitsPerSample() != props.bitsPerSample };

        track.setDuration(props.duration);
        track.setContainer(props.container);
        track.setCodec(props.codec);
        track.setBitrate(props.bitrate);
        track.setChannelCount(props.channelCount);
        track.setSampleRate(props.sampleRate);
        track.setBitsPerSample(props.bitsPerSample);

        return changed;
    }

    AudioFileScanOperation::OperationResult AudioFileScanOperation::processResult()
    {
        LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessAudioScanData");

        db::ISession& dbSession{ getDb().getTLSSession() };
        std::optional<db::Track> track{ db::Track::findByPath(dbSession, getFilePath()) };
        const auto removeTrack{ [&](db::TrackId trackId) {
            db::Track::destroy(dbSession, std::span{ &trackId, 1 });
        } };
        if (!_file)
        {
            if (track)
            {
                removeTrack(track->getId());
                LMS_LOG(DBUPDATER, DEBUG, "Removed track " << getFilePath());
                return OperationResult::Removed;
            }
            return OperationResult::Skipped;
        }

        if (_file->track.mbid && (!track || getScannerSettings().skipDuplicateTrackMBID))
        {
            const std::vector<db::Track> duplicateTracks{ db::Track::findByMBID(dbSession, *_file->track.mbid) };

            // find for an existing track MBID as the file may have just been moved
            if (!track && duplicateTracks.size() == 1)
            {
                const db::Track& otherTrack{ duplicateTracks.front() };
                std::error_code ec;
                if (!std::filesystem::exists(otherTrack.getAbsoluteFilePath(), ec))
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Considering track " << getFilePath() << " moved from " << otherTrack.getAbsoluteFilePath());
                    track = otherTrack;
                    track->setAbsoluteFilePath(getFilePath());
                }
            }

            // Skip duplicate track MBID
            if (getScannerSettings().skipDuplicateTrackMBID)
            {
                for (const db::Track& otherTrack : duplicateTracks)
                {
                    // Skip ourselves
                    if (track && track->getId() == otherTrack.getId())
                        continue;

                    // If the other file is no longer in an active library it will be removed later:
                    // do not treat the current file as a duplicate, otherwise no file will remain for this MBID
                    const std::filesystem::path otherFilePath{ otherTrack.getAbsoluteFilePath() };
                    const auto isInActiveLibrary{ [&](const MediaLibraryInfo& libraryInfo) {
                        if (!core::pathUtils::isPathInRootPath(otherFilePath, libraryInfo.rootDirectory))
                            return false;
                        return libraryInfo.ignoreRules.isEmpty() || !libraryInfo.ignoreRules.isIgnored(std::filesystem::relative(otherFilePath, libraryInfo.rootDirectory), IgnoreRules::IsDirectory{ false });
                    } };
                    const auto& mediaLibraries{ getScannerSettings().mediaLibraries };
                    if (!std::any_of(std::cbegin(mediaLibraries), std::cend(mediaLibraries), isInActiveLibrary))
                        continue;

                    LMS_LOG(DBUPDATER, DEBUG, "Skipped " << getFilePath() << ": same MBID already found in " << otherTrack.getAbsoluteFilePath());
                    // As this MBID already exists, just remove what we just scanned
                    if (track)
                    {
                        removeTrack(track->getId());

                        LMS_LOG(DBUPDATER, DEBUG, "Removed " << getFilePath() << ": same MBID already found in " << otherTrack.getAbsoluteFilePath());
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
                track->setAbsoluteFilePath(getFilePath());
            }
        }

        // We estimate this is an audio file if the duration is not null
        if (_file->audioProperties.duration == std::chrono::milliseconds::zero())
        {
            addError<BadAudioDurationError>(getFilePath());

            if (track)
            {
                removeTrack(track->getId());
                LMS_LOG(DBUPDATER, DEBUG, "Removed track " << getFilePath());
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
            track = db::Track::create(dbSession);
            added = true;

            track->setAbsoluteFilePath(getFilePath());
            track->setAddedTime(getMediaLibrary().firstScan ? getLastWriteTime() : Wt::WDateTime::currentDateTime()); // may be erased by encodingTime
        }

        // Track related data
        assert(track);
        track->setScanVersion(getScannerSettings().audioScanVersion);

        // Audio properties
        const bool audioPropertiesChanged{ updateAudioProperties(*track, _file->audioProperties) };

        track->setFileSize(getFileSize());
        track->setLastWriteTime(getLastWriteTime());

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
                track->setAddedTime(time.isValid() ? Wt::WDateTime{ date, time } : Wt::WDateTime{ date });
        }

        std::optional<db::MediaLibrary> mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this
        track->setMediaLibrary(mediaLibrary ? mediaLibrary->getId() : db::MediaLibraryId{});
        const db::Directory directory{ utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary) };
        track->setDirectory(directory.getId());

        const helpers::AllowFallbackOnMBIDEntry allowFallback{ getScannerSettings().allowArtistMBIDFallback };

        // For now, alway tie a medium to a release, and a release mst have at least one medium, even if no disc number is set
        if (_file->track.medium && _file->track.medium->release)
        {
            const db::ReleaseId releaseId{ getOrCreateRelease(dbSession, *_file->track.medium->release, directory, allowFallback) };
            assert(releaseId.isValid());
            track->setRelease(releaseId);
            track->setMedium(getOrCreateMedium(dbSession, *_file->track.medium, releaseId));
        }
        else
        {
            track->setRelease({});
            track->setMedium({});
        }

        const db::TrackId trackId{ track->getId() };
        db::Track::clearArtistLinks(dbSession, trackId);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Artist, _file->track.artists, allowFallback);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Conductor, _file->track.conductorArtists, allowFallback);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Composer, _file->track.composerArtists, allowFallback);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Lyricist, _file->track.lyricistArtists, allowFallback);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Mixer, _file->track.mixerArtists, allowFallback);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Producer, _file->track.producerArtists, allowFallback);
        createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Remixer, _file->track.remixerArtists, allowFallback);

        for (const auto& [role, performers] : _file->track.performerArtists)
            createTrackArtistLinks(dbSession, trackId, db::TrackArtistLinkType::Performer, role, performers, allowFallback);

        db::Track::setClusters(dbSession, trackId, getOrCreateClusters(dbSession, _file->track));
        db::Track::setGenres(dbSession, trackId, getOrCreateGenres(dbSession, _file->track.genres));
        db::Track::setGroupings(dbSession, trackId, getOrCreateGroupings(dbSession, _file->track.groupings));
        db::Track::setLanguages(dbSession, trackId, getOrCreateLanguages(dbSession, _file->track.languages));
        db::Track::setMoods(dbSession, trackId, getOrCreateMoods(dbSession, _file->track.moods));
        db::Track::setWorks(dbSession, trackId, getOrCreateWorks(dbSession, track->getReleaseId(), _file->track.works));

        db::Track::clearMovements(dbSession, trackId);
        for (const auto& movement : _file->track.movements)
            db::Movement::create(dbSession, movement.name, movement.number, movement.count, trackId);

        track->setName(title);
        track->setTrackNumber(_file->track.position);
        track->setDate(_file->track.date);
        track->setOriginalDate(_file->track.originalDate);
        if (!track->getOriginalDate().isValid() && _file->track.originalYear)
            track->setOriginalDate(core::PartialDateTime{ *_file->track.originalYear });

        // If a file has an OriginalDate but no date, set it to ease filtering
        if (!_file->track.date.isValid() && _file->track.originalDate.isValid())
            track->setDate(_file->track.originalDate);

        track->setRecordingMBID(_file->track.recordingMBID);
        track->setTrackMBID(_file->track.mbid);
        if (audioPropertiesChanged)
        {
            if (const auto musicnnEmbedding{ db::TrackMusicNNEmbeddings::find(dbSession, track->getId()) })
                db::TrackMusicNNEmbeddings::destroy(dbSession, std::array{ musicnnEmbedding->getId() });
        }
        track->setCopyright(_file->track.copyright);
        track->setCopyrightURL(_file->track.copyrightURL);
        track->setAdvisory(getAdvisory(_file->track.advisory));
        track->setComment(!_file->track.comments.empty() ? _file->track.comments.front() : ""); // only take the first one for now
        track->setReplayGain(_file->track.replayGain);
        track->setArtistDisplayName(_file->track.artistDisplayName);

        db::Track::clearEmbeddedLyrics(dbSession, trackId);
        for (const Lyrics& lyricsInfo : _file->track.lyrics)
            createLyrics(dbSession, trackId, lyricsInfo);

        updateEmbeddedImages(dbSession, trackId, _file->images);

        db::Track::update(dbSession, *track);

        if (added)
        {
            LMS_LOG(DBUPDATER, DEBUG, "Added audio file " << getFilePath());
            return OperationResult::Added;
        }

        LMS_LOG(DBUPDATER, DEBUG, "Updated audio file " << getFilePath());
        return OperationResult::Updated;
    }
} // namespace lms::scanner
