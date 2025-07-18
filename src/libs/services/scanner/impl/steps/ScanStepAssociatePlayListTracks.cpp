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

#include "ScanStepAssociatePlayListTracks.hpp"

#include <deque>
#include <filesystem>
#include <span>

#include "core/IJob.hpp"
#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"
#include "services/scanner/ScanErrors.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"

namespace lms::scanner
{
    namespace
    {
        struct TrackInfo
        {
            db::TrackId trackId;
            db::ReleaseId releaseId;
        };

        struct PlayListFileAssociation
        {
            db::PlayListFileId playListFileIdId;

            std::vector<TrackInfo> tracks;
        };
        using PlayListFileAssociationContainer = std::deque<PlayListFileAssociation>;

        db::Track::pointer getMatchingTrack(db::Session& session, const std::filesystem::path& filePath, const db::Directory::pointer& playListDirectory)
        {
            db::Track::pointer matchingTrack;
            if (filePath.is_absolute())
            {
                matchingTrack = db::Track::findByPath(session, filePath);
            }
            else
            {
                const std::filesystem::path absolutePath{ playListDirectory->getAbsolutePath() / filePath };
                matchingTrack = db::Track::findByPath(session, absolutePath.lexically_normal());
            }

            return matchingTrack;
        }

        bool isSingleReleasePlayList(std::span<const TrackInfo> tracks)
        {
            if (tracks.empty())
                return true;

            const db::ReleaseId releaseId{ tracks.front().releaseId };
            return std::all_of(std::cbegin(tracks) + 1, std::cend(tracks), [=](const TrackInfo& trackInfo) { return trackInfo.releaseId == releaseId; });
        }

        bool trackListNeedsUpdate(db::Session& session, std::string_view name, std::span<const TrackInfo> tracks, const db::TrackList::pointer& trackList)
        {
            if (trackList->getName() != name)
                return true;

            db::TrackListEntry::FindParameters params;
            params.setTrackList(trackList->getId());

            bool needUpdate{};
            std::size_t currentIndex{};
            db::TrackListEntry::find(session, params, [&](const db::TrackListEntry::pointer& entry) {
                if (currentIndex > tracks.size() || tracks[currentIndex].trackId != entry->getTrackId())
                    needUpdate = true;

                currentIndex += 1;
            });

            if (currentIndex != tracks.size())
                needUpdate = true;

            return needUpdate;
        }

        void updatePlayList(db::Session& session, const PlayListFileAssociation& playListFileAssociation)
        {
            db::PlayListFile::pointer playListFile{ db::PlayListFile::find(session, playListFileAssociation.playListFileIdId) };
            assert(playListFile);

            db::TrackList::pointer trackList{ playListFile->getTrackList() };
            if (playListFileAssociation.tracks.empty())
            {
                if (trackList)
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Removed associated tracklist for " << playListFile->getAbsoluteFilePath() << "");
                    trackList.remove();
                }

                return;
            }

            const bool createTrackList{ !trackList };
            if (createTrackList)
            {
                trackList = session.create<db::TrackList>(playListFile->getName(), db::TrackListType::PlayList);
                playListFile.modify()->setTrackList(trackList);
            }

            trackList.modify()->setVisibility(db::TrackList::Visibility::Public);
            trackList.modify()->setLastModifiedDateTime(playListFile->getLastWriteTime());
            trackList.modify()->setName(playListFile->getName());

            trackList.modify()->clear();
            for (const TrackInfo trackInfo : playListFileAssociation.tracks)
            {
                // TODO no need to fetch the whole track for that
                if (db::Track::pointer track{ db::Track::find(session, trackInfo.trackId) })
                    session.create<db::TrackListEntry>(track, trackList, playListFile->getLastWriteTime());
            }

            LMS_LOG(DBUPDATER, DEBUG, std::string_view{ createTrackList ? "Created" : "Updated" } << " associated tracklist for " << playListFile->getAbsoluteFilePath() << " (" << playListFileAssociation.tracks.size() << " tracks)");
        }

        void updatePlayLists(db::Session& session, PlayListFileAssociationContainer& playListFileAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 5 };

            while ((forceFullBatch && playListFileAssociations.size() >= writeBatchSize) || !playListFileAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !playListFileAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updatePlayList(session, playListFileAssociations.front());
                    playListFileAssociations.pop_front();
                }
            }
        }

        bool fetchNextPlayListFileIdRange(db::Session& session, db::PlayListFileId& lastPlayListFileId, db::IdRange<db::PlayListFileId>& idRange)
        {
            constexpr std::size_t readBatchSize{ 100 };

            auto transaction{ session.createReadTransaction() };

            idRange = db::PlayListFile::findNextIdRange(session, lastPlayListFileId, readBatchSize);
            lastPlayListFileId = idRange.last;

            return idRange.isValid();
        }

        class ComputePlayListFileAssociationsJob : public core::IJob
        {
        public:
            ComputePlayListFileAssociationsJob(db::IDb& db, const ScannerSettings& settings, db::IdRange<db::PlayListFileId> playListFileIdRange)
                : _db{ db }
                , _settings{ settings }
                , _playListFileIdRange{ playListFileIdRange }
            {
            }

            std::span<const PlayListFileAssociation> getAssociations() const { return _associations; }
            std::size_t getProcessedCount() const { return _processedCount; }
            std::span<const std::shared_ptr<ScanError>> getErrors() const { return _errors; }

        private:
            core::LiteralString getName() const override { return "Associate PlayList Tracks"; }
            void run() override
            {
                auto& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::PlayListFile::find(session, _playListFileIdRange, [&](const db::PlayListFile::pointer& playListFile) {
                    PlayListFileAssociation playListAssociation;

                    playListAssociation.playListFileIdId = playListFile->getId();

                    std::vector<std::shared_ptr<ScanError>> pendingErrors;

                    const auto& files{ playListFile->getFiles() };
                    for (const std::filesystem::path& file : files)
                    {
                        // TODO optim: no need to fetch the whole track
                        const db::Track::pointer track{ getMatchingTrack(session, file, playListFile->getDirectory()) };
                        if (track)
                            playListAssociation.tracks.push_back(TrackInfo{ .trackId = track->getId(), .releaseId = track->getReleaseId() });
                        else
                            pendingErrors.emplace_back(std::make_shared<PlayListFilePathMissingError>(playListFile->getAbsoluteFilePath(), file));
                    }

                    if (pendingErrors.size() == files.size())
                    {
                        pendingErrors.clear();
                        pendingErrors.emplace_back(std::make_shared<PlayListFileAllPathesMissingError>(playListFile->getAbsoluteFilePath()));
                    }
                    _errors.insert(std::end(_errors), std::begin(pendingErrors), std::end(pendingErrors));

                    if (playListAssociation.tracks.empty()
                        || (_settings.skipSingleReleasePlayLists && isSingleReleasePlayList(playListAssociation.tracks)))
                    {
                        playListAssociation.tracks.clear();
                    }

                    bool needUpdate{ true };
                    if (const db::TrackList::pointer trackList{ playListFile->getTrackList() })
                    {
                        if (!playListAssociation.tracks.empty())
                            needUpdate = trackListNeedsUpdate(session, playListFile->getName(), playListAssociation.tracks, trackList);
                    }
                    else if (playListAssociation.tracks.empty())
                        needUpdate = false;

                    if (needUpdate)
                        _associations.emplace_back(std::move(playListAssociation));

                    _processedCount++;
                });
            }

            db::IDb& _db;
            const ScannerSettings& _settings;
            db::IdRange<db::PlayListFileId> _playListFileIdRange;
            std::vector<PlayListFileAssociation> _associations;
            std::vector<std::shared_ptr<ScanError>> _errors;
            std::size_t _processedCount{};
        };

    } // namespace

    bool ScanStepAssociatePlayListTracks::needProcess(const ScanContext& context) const
    {
        if (context.stats.getChangesCount() > 0)
            return true;

        if (getLastScanSettings() && getLastScanSettings()->skipSingleReleasePlayLists != _settings.skipSingleReleasePlayLists)
            return true;

        return false;
    }

    void ScanStepAssociatePlayListTracks::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::PlayListFile::getCount(session);
        }

        PlayListFileAssociationContainer playListTrackAssociations;
        auto processJobsDone = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& associationJob{ static_cast<const ComputePlayListFileAssociationsJob&>(*job) };
                const auto& associations{ associationJob.getAssociations() };

                playListTrackAssociations.insert(std::end(playListTrackAssociations), std::cbegin(associations), std::cend(associations));
                for (const std::shared_ptr<ScanError>& error : associationJob.getErrors())
                    addError(context, error);

                context.currentStepStats.processedElems += associationJob.getProcessedCount();
            }

            updatePlayLists(session, playListTrackAssociations, true);
            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), 20, processJobsDone, 1, 0.85F };

            db::PlayListFileId lastPlayListFileId;
            db::IdRange<db::PlayListFileId> playListFileIdRange;
            while (fetchNextPlayListFileIdRange(session, lastPlayListFileId, playListFileIdRange))
                queue.push(std::make_unique<ComputePlayListFileAssociationsJob>(_db, _settings, playListFileIdRange));
        }

        // process all remaining associations
        updatePlayLists(session, playListTrackAssociations, false);
    }
} // namespace lms::scanner
