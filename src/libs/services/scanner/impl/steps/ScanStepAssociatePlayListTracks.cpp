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

#include "core/ILogger.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/PlayListFile.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"

namespace lms::scanner
{
    namespace
    {
        constexpr std::size_t readBatchSize{ 20 };
        constexpr std::size_t writeBatchSize{ 5 };

        struct PlayListFileAssociation
        {
            db::PlayListFileId playListFileIdId;
            std::vector<db::TrackId> trackIds;
        };
        using PlayListFileAssociationContainer = std::deque<PlayListFileAssociation>;

        struct SearchPlayListFileContext
        {
            db::Session& session;
            db::PlayListFileId lastRetrievedPlayListFileId;
            std::size_t processedPlayListFileCount{};
        };

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

        bool trackListNeedsUpdate(db::Session& session, std::string_view name, std::span<const db::TrackId> trackIds, const db::TrackList::pointer& trackList)
        {
            if (trackList->getName() != name)
                return true;

            db::TrackListEntry::FindParameters params;
            params.setTrackList(trackList->getId());

            bool needUpdate{};
            std::size_t currentIndex{};
            db::TrackListEntry::find(session, params, [&](const db::TrackListEntry::pointer& entry) {
                if (currentIndex > trackIds.size() || trackIds[currentIndex] != entry->getTrackId())
                    needUpdate = true;

                currentIndex += 1;
            });

            if (currentIndex != trackIds.size())
                needUpdate = true;

            return needUpdate;
        }

        bool fetchNextPlayListFilesToUpdate(SearchPlayListFileContext& searchContext, PlayListFileAssociationContainer& playListFileAssociations)
        {
            const db::PlayListFileId playListFileIdId{ searchContext.lastRetrievedPlayListFileId };

            {
                auto transaction{ searchContext.session.createReadTransaction() };

                db::PlayListFile::find(searchContext.session, searchContext.lastRetrievedPlayListFileId, readBatchSize, [&](const db::PlayListFile::pointer& playListFile) {
                    PlayListFileAssociation playListAssociation;

                    playListAssociation.playListFileIdId = playListFile->getId();

                    const auto files{ playListFile->getFiles() };
                    for (const std::filesystem::path& file : files)
                    {
                        // TODO optim: no need to fetch the whole track
                        db::Track::pointer track{ getMatchingTrack(searchContext.session, file, playListFile->getDirectory()) };
                        if (track)
                            playListAssociation.trackIds.push_back(track->getId());
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Track '" << file.string() << "' not found in playlist '" << playListFile->getAbsoluteFilePath().string() << "'");
                    }

                    bool needUpdate{ true };
                    if (const db::TrackList::pointer trackList{ playListFile->getTrackList() })
                        needUpdate = trackListNeedsUpdate(searchContext.session, playListFile->getName(), playListAssociation.trackIds, trackList);

                    if (needUpdate)
                    {
                        LMS_LOG(DBUPDATER, DEBUG, "Updating PlayList '" << playListFile->getAbsoluteFilePath().string() << "' (" << playListAssociation.trackIds.size() << " files)");
                        playListFileAssociations.emplace_back(std::move(playListAssociation));
                    }
                    searchContext.processedPlayListFileCount++;
                });
            }

            return playListFileIdId != searchContext.lastRetrievedPlayListFileId;
        }

        void updatePlayListFile(db::Session& session, const PlayListFileAssociation& playListFileAssociation)
        {
            db::PlayListFile::pointer playListFile{ db::PlayListFile::find(session, playListFileAssociation.playListFileIdId) };
            assert(playListFile);

            db::TrackList::pointer trackList{ playListFile->getTrackList() };
            if (!trackList)
            {
                trackList = session.create<db::TrackList>(playListFile->getName(), db::TrackListType::PlayList);
                playListFile.modify()->setTrackList(trackList);
            }

            trackList.modify()->setVisibility(db::TrackList::Visibility::Public);
            trackList.modify()->setLastModifiedDateTime(playListFile->getLastWriteTime());
            trackList.modify()->setName(playListFile->getName());

            trackList.modify()->clear();
            for (const db::TrackId trackId : playListFileAssociation.trackIds)
            {
                if (db::Track::pointer track{ db::Track::find(session, trackId) })
                    session.create<db::TrackListEntry>(track, trackList, playListFile->getLastWriteTime());
            }
        }

        void updatePlayListFiles(db::Session& session, PlayListFileAssociationContainer& playListFileAssociations)
        {
            while (!playListFileAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !playListFileAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updatePlayListFile(session, playListFileAssociations.front());
                    playListFileAssociations.pop_front();
                }
            }
        }
    } // namespace

    void ScanStepAssociatePlayListTracks::process(ScanContext& context)
    {
        if (_abortScan)
            return;

        if (context.stats.nbChanges() == 0)
            return;

        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::PlayListFile::getCount(session);
        }

        SearchPlayListFileContext searchContext{
            .session = session,
            .lastRetrievedPlayListFileId = {},
        };

        PlayListFileAssociationContainer playListFileAssociations;
        while (fetchNextPlayListFilesToUpdate(searchContext, playListFileAssociations))
        {
            if (_abortScan)
                return;

            updatePlayListFiles(session, playListFileAssociations);
            context.currentStepStats.processedElems = searchContext.processedPlayListFileCount;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
