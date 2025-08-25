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

#include "CueFileScanOperation.hpp"

#include <cmath>
#include <fstream>
#include <ranges>

#include "ScannerSettings.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/PseudoProtocols.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "metadata/IAudioFileParser.hpp"
#include "metadata/Types.hpp"
#include "scanners/Utils.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "database/objects/Artist.hpp"
#include "helpers/ArtistHelpers.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/Medium.hpp"

namespace lms::scanner
{
    namespace
    {
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
        db::Medium::pointer getOrCreateMedium(db::Session& session, const metadata::Medium& medium, const db::Release::pointer& release)
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
    }

    namespace cue {
        using Frames = std::chrono::duration<std::size_t, std::ratio<1, 75>>; // 75 "frames" per second

        struct ParsedCueTrack
        {
            std::filesystem::path            baseFile;
            std::size_t                      index      = 0;
            std::string                      title;
            std::string                      discTitle;
            std::string                      performer;
            Frames                           startFrame = Frames::max();
            Frames                           endFrame   = Frames::zero();
            std::unique_ptr<metadata::Track> parsedTrack;
        };
    }

    CueFileScanOperation::CueFileScanOperation(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings, metadata::IAudioFileParser& parser)
        : FileScanOperationBase{ std::move(fileToScan), db, settings }
        , _parser(parser)
    {
    }

    CueFileScanOperation::~CueFileScanOperation() = default;

    void CueFileScanOperation::scan()
    {
        _parsedFile.clear();

        // Parse CUE file
        std::ifstream cueFile{ getFilePath() };
        if (!cueFile.good()) {
            addError<CueFileError>("could not open CUE file");
            return;
        }

        using TagDataPair = std::pair<std::string, std::string>;
        std::vector<TagDataPair> parsedCue;

        static constexpr auto unquote = [] (std::string_view str) {
            if (str.size() >= 2 && str.starts_with("\"") && str.ends_with("\"")) {
                return str.substr(1, str.size() - 2);
            }
            return str;
        };

        std::stringstream tag{};
        std::stringstream data{};
        bool readTag = true;
        for (char c = 0; !cueFile.get(c).eof();) {
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                parsedCue.emplace_back(tag.str(), data.str());
                readTag = true;
                tag.str("");  tag.clear();
                data.str(""); data.clear();
                continue;
            }

            if (readTag && c == ' ' && tag.str() != "REM") {
                if (!tag.str().empty()) {
                    readTag = false;
                }
                continue;
            }

            (readTag ? tag : data) << c;
        }

        struct Disc {
            std::string performer;
            std::string title;
        } disc;
        disc.title = getFilePath().filename().string();

        std::filesystem::path currentFile;

        static constexpr auto* digits{ "0123456789" };

        const std::unordered_map<std::string, std::function<void(std::string&&)>> sw{
            {
                "PERFORMER", [this, &disc] (const std::string& arg) {
                    if (arg == "\"\"") return;

                    if (_parsedFile.empty()) {
                        disc.performer = unquote(arg);
                    } else {
                        _parsedFile.back().performer = unquote(arg);
                    }
                }
            },
            {
                "TITLE", [this, &disc] (const std::string& arg) {
                    if (arg == "\"\"") return;

                    if (_parsedFile.empty()) {
                        disc.title = unquote(arg);
                    } else {
                        _parsedFile.back().title = unquote(arg);
                    }
                }
            },
            {
                "FILE", [this, &currentFile] (std::string_view name) {
                    name = name.substr(1);
                    const auto idx = name.rfind('"');
                    if (idx == std::string_view::npos) {
                        addError<CueFileError>("wrong FILE filed format in .cue file");
                        return;
                    }
                    name = name.substr(0, idx);

                    // Look for the referred to file in the same directory
                    currentFile = getFilePath().parent_path() / name;
                    if (!std::filesystem::exists(currentFile)) {
                        addError<CueFileError>("could not find the referred to file `" + std::string{name} + "` in the same directory");
                        return;
                    }
                }
            },
            {
                "TRACK", [this, &disc, &currentFile] (std::string_view arg) {
                    if (currentFile.empty()) {
                        addError<CueFileError>(getFilePath(), "track `", arg, "` does not belong to a file");
                        return;
                    }
                    const auto pos = arg.find(' ');
                    if (pos == std::string_view::npos || arg.substr(pos + 1) != "AUDIO") {
                        addError<CueFileError>(getFilePath(), "track `", arg, "` is not an AUDIO track, got `", arg.substr(pos + 1), "` instead");
                        return;
                    }
                    const std::string idx{ arg.substr(0, pos) };
                    if (idx.find_first_not_of(digits) != std::string_view::npos) {
                        addError<CueFileError>(getFilePath(), "track `", arg, "` index is not an integer");
                        return;
                    }
                    auto& newTrack = _parsedFile.emplace_back();
                    newTrack.discTitle = disc.title;
                    newTrack.performer = disc.performer;
                    newTrack.baseFile  = currentFile;
                    // FIXME: Is it reasonable to bounds-check here or are we safe?
                    newTrack.index     = std::stoll(idx);
                }
            },
            {
                "INDEX", [this, &disc, &currentFile] (std::string_view arg) {
                    if (_parsedFile.empty()) {
                        addError<CueFileError>(getFilePath(), "index `", arg, "` not in track");
                        return;
                    }

                    const auto pos = arg.find(' ');
                    if (pos == std::string_view::npos) {
                        addError<CueFileError>(getFilePath(), "wrong index format: `", arg, "`");
                        return;
                    }
                    const auto index = arg.substr(0, pos);
                    if (index.find_first_not_of(digits) != std::string_view::npos) {
                        addError<CueFileError>(getFilePath(), "index `", index, "` is not an integer");
                        return;
                    }
                    const auto point = arg.substr(pos + 1);
                    auto parsedPoint = std::views::split(point, ':')
                                     | std::views::transform([] (auto part) { return std::string_view( part.begin(), part.end() ); });
                    auto parsedIt = parsedPoint.begin();

                    auto result = cue::Frames::zero();
                    for (std::size_t i = 0; i < 3; ++i, ++parsedIt) {
                        if (parsedIt == parsedPoint.end()) {
                            addError<CueFileError>(getFilePath(), "cannot parse time point `", point, "`");
                            return;
                        }
                        const std::string view{ *parsedIt };
                        if (view.find_first_not_of(digits) != std::string::npos) {
                            addError<CueFileError>(getFilePath(), "cannot parse time point `", point, "`");
                            return;
                        }
                        const std::size_t val = std::stoll(view);
                        switch (i) {
                        case 0:
                            result += std::chrono::minutes(val);
                            break;
                        case 1:
                            result += std::chrono::seconds(val);
                            break;
                        case 2:
                            result += cue::Frames(val);
                            break;
                        default: assert(false);
                        }
                    }

                    auto& track = _parsedFile.back();
                    if (index == "00" || index == "01") {
                        track.startFrame = std::min(track.startFrame, result);
                        // If there was a previous track without an end frame on
                        // the same file, do it too
                        if (_parsedFile.size() < 2) {
                            return;
                        }
                        auto& prevTrack = _parsedFile[_parsedFile.size() - 2];
                        if (prevTrack.baseFile != track.baseFile) {
                            return;
                        }
                        if (prevTrack.endFrame != cue::Frames::zero()) {
                            return;
                        }
                        prevTrack.endFrame = track.startFrame;
                    } else {
                        track.endFrame = std::max(track.endFrame, result);
                    }
                }
            },
        };

        for (auto [ tag, data ] : parsedCue) {
            if (const auto it = sw.find(tag); it != sw.cend()) {
                it->second(std::move(data));
            }

            if (!getErrors().empty()) {
                break;
            }
        }

        for (auto& track : _parsedFile) {
            // Will parse the same file multiple times - completely unnecessary
            try {
                track.parsedTrack = _parser.parseMetaData(track.baseFile);
            } catch (...) {
                // If file parsing failed - not to worry, we will just skip
                // the track - CUE files may point to corrupted data, shit
                // happens
                track.parsedTrack = nullptr;
            }
        }
    }

    CueFileScanOperation::OperationResult CueFileScanOperation::processResult()
    {
        LMS_SCOPED_TRACE_DETAILED("Scanner", "ProcessCueScanData");

        const auto now = std::chrono::system_clock::now();

        if (_parsedFile.empty()) {
            LMS_LOG(DBUPDATER, ERROR, "Had errors parsing " << getFilePath() << ", skipping");
            return OperationResult::Skipped;
        }

        db::Session& dbSession{ getDb().getTLSSession() };
        auto mediaLibrary{ db::MediaLibrary::find(dbSession, getMediaLibrary().id) }; // may be null if settings are updated in // => next scan will correct this

        // Each .cue file represents a release - so generate a release from it
        for (const auto& input : _parsedFile) {
            LMS_LOG(DBUPDATER, DEBUG, "Adding CUE track " << input.title);

            namespace stdc = std::chrono;

            if (input.parsedTrack == nullptr) {
                continue;
            }
            if (input.parsedTrack->audioProperties.duration == std::chrono::milliseconds::zero()) {
                addError<BadAudioDurationError>(input.baseFile);
                return OperationResult::Skipped;
            }

            const auto start = input.startFrame == cue::Frames::max()
                             ? stdc::milliseconds::zero()
                             : stdc::duration_cast<stdc::milliseconds>(input.startFrame);
            const auto end   = input.endFrame == cue::Frames::zero()
                             ? input.parsedTrack->audioProperties.duration
                             : stdc::duration_cast<stdc::milliseconds>(input.endFrame);

            const core::TrackOn::DecipheredURI dURI{
                .path     = input.baseFile,
                .start    = start,
                .duration = end - start,
            };

            auto track = dbSession.create<db::Track>();

            track.modify()->setDuration(dURI.duration);
            track.modify()->setAbsoluteFilePath(core::track_on.encode(dURI));
            track.modify()->setAddedTime(getMediaLibrary().firstScan ? getLastWriteTime() : Wt::WDateTime::currentDateTime()); // may be erased by encodingTime

            assert(track);
            track.modify()->setScanVersion(getScannerSettings().audioScanVersion);

            // Audio properties
            track.modify()->setBitrate(input.parsedTrack->audioProperties.bitrate);
            track.modify()->setBitsPerSample(input.parsedTrack->audioProperties.bitsPerSample);
            track.modify()->setChannelCount(input.parsedTrack->audioProperties.channelCount);
            track.modify()->setSampleRate(input.parsedTrack->audioProperties.sampleRate);

            track.modify()->setFileSize(getFileSize());
            track.modify()->setLastWriteTime(now); // sign all tracks with the same time

            track.modify()->setMediaLibrary(mediaLibrary);
            db::Directory::pointer directory{ utils::getOrCreateDirectory(dbSession, getFilePath().parent_path(), mediaLibrary) };
            track.modify()->setDirectory(directory);

            track.modify()->setName(input.title);
            track.modify()->setTrackNumber(input.index);

            //track.modify()->setReleaseName(input.discTitle);
            {
            metadata::Release rel;
            rel.name              = input.discTitle;
            rel.artistDisplayName = input.performer;
            rel.artists           = { metadata::Artist(input.performer) };
            auto release{ getOrCreateRelease(dbSession, rel, directory) };
            assert(release);
            track.modify()->setRelease(release);
            track.modify()->setMedium(getOrCreateMedium(dbSession, metadata::Medium{ .name = input.discTitle, .release = rel }, release));
            }

            {
            track.modify()->setArtistDisplayName(input.performer);
            const helpers::AllowFallbackOnMBIDEntry allowFallback{ getScannerSettings().allowArtistMBIDFallback };
            auto artist = helpers::getOrCreateArtistByName(dbSession, metadata::Artist(input.performer), allowFallback);
            auto link = dbSession.create<db::TrackArtistLink>(track, artist, db::TrackArtistLinkType::Artist, std::string_view{}, false);
            link.modify()->setArtistName(input.performer);
            link = dbSession.create<db::TrackArtistLink>(track, artist, db::TrackArtistLinkType::ReleaseArtist, std::string_view{}, false);
            link.modify()->setArtistName(input.performer);
            }
        }

        return OperationResult::Added;
    }
} // namespace lms::scanner
