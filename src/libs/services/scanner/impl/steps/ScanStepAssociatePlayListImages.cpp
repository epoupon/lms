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

#include "ScanStepAssociatePlayListImages.hpp"

#include <deque>
#include <span>

#include "core/IJob.hpp"
#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/PlayListFile.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"

namespace lms::scanner
{
    namespace
    {
        struct PlayListArtworkAssociation
        {
            db::PlayListFileId playListFileId;
            db::ArtworkId artworkId; // invalid = clear
        };
        using PlayListArtworkAssociationContainer = std::deque<PlayListArtworkAssociation>;

        std::optional<db::Artwork> computePreferredArtwork(db::ISession& session, const db::PlayListFile& playListFile)
        {
            // Priority 1: explicit #EXTIMG: hint stored in the playlist file
            const std::filesystem::path& coverImageFile{ playListFile.getCoverImageFile() };
            if (!coverImageFile.empty())
            {
                const std::filesystem::path coverImagePath{ coverImageFile.is_relative() ? (db::Directory::find(session, playListFile.getDirectoryId())->getAbsolutePath() / coverImageFile).lexically_normal() : coverImageFile };

                const std::optional<db::Image> image{ db::Image::find(session, coverImagePath) };
                if (image)
                    return db::Artwork::find(session, image->getId());
            }

            // Priority 2: image file with the same stem as the playlist in the same directory
            std::optional<db::Artwork> artwork;
            db::Image::FindParameters params;
            params.setDirectory(playListFile.getDirectoryId());
            params.setFileStem(playListFile.getAbsoluteFilePath().stem().string());

            db::Image::find(session, params, [&](const db::Image& image) {
                if (!artwork)
                    artwork = db::Artwork::find(session, image.getId());
            });

            return artwork;
        }

        void updatePlayListPreferredArtworks(db::ISession& session, PlayListArtworkAssociationContainer& associations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while ((forceFullBatch && associations.size() >= writeBatchSize) || (!forceFullBatch && !associations.empty()))
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !associations.empty() && i < writeBatchSize; ++i)
                {
                    const auto& assoc{ associations.front() };
                    db::PlayListFile::updatePreferredArtwork(session, assoc.playListFileId, assoc.artworkId);
                    associations.pop_front();
                }
            }
        }

        bool fetchNextPlayListIdRange(db::ISession& session, db::PlayListFileId& lastRetrievedId, db::IdRange<db::PlayListFileId>& idRange)
        {
            constexpr std::size_t readBatchSize{ 100 };

            auto transaction{ session.createReadTransaction() };

            idRange = db::PlayListFile::findNextIdRange(session, lastRetrievedId, readBatchSize);
            lastRetrievedId = idRange.last;

            return idRange.isValid();
        }

        class ComputePlayListArtworkAssociationsJob : public core::IJob
        {
        public:
            ComputePlayListArtworkAssociationsJob(db::IDb& db, db::IdRange<db::PlayListFileId> idRange)
                : _db{ db }
                , _idRange{ idRange }
            {
            }
            ~ComputePlayListArtworkAssociationsJob() override = default;
            ComputePlayListArtworkAssociationsJob(const ComputePlayListArtworkAssociationsJob&) = delete;
            ComputePlayListArtworkAssociationsJob& operator=(const ComputePlayListArtworkAssociationsJob&) = delete;

            std::span<const PlayListArtworkAssociation> getAssociations() const { return _associations; }
            std::size_t getProcessedCount() const { return _processedCount; }

        private:
            core::LiteralString getName() const override { return "Associate PlayList Artworks"; }
            void run() override
            {
                auto& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::PlayListFile::find(session, _idRange, [this, &session](const db::PlayListFile& playListFile) {
                    const std::optional<db::Artwork> preferredArtwork{ computePreferredArtwork(session, playListFile) };
                    const db::ArtworkId newId{ preferredArtwork ? preferredArtwork->getId() : db::ArtworkId{} };

                    if (newId != playListFile.getPreferredArtworkId())
                    {
                        _associations.push_back({ playListFile.getId(), newId });

                        if (preferredArtwork)
                            LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork for playlist '" << playListFile.getName() << "' with image " << db::Artwork::getAbsoluteFilePath(session, preferredArtwork->getId()));
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Removing preferred artwork from playlist '" << playListFile.getName() << "'");
                    }

                    _processedCount++;
                });
            }

            db::IDb& _db;
            db::IdRange<db::PlayListFileId> _idRange;
            std::vector<PlayListArtworkAssociation> _associations;
            std::size_t _processedCount{};
        };

    } // namespace

    bool ScanStepAssociatePlayListImages::needProcess(const ScanContext& context) const
    {
        return context.stats.getChangesCount() > 0;
    }

    void ScanStepAssociatePlayListImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::PlayListFile::getCount(session);
        }

        PlayListArtworkAssociationContainer associations;
        auto processJobsDone = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& associationJob{ static_cast<const ComputePlayListArtworkAssociationsJob&>(*job) };

                const auto& jobAssociations{ associationJob.getAssociations() };
                associations.insert(std::end(associations), std::cbegin(jobAssociations), std::cend(jobAssociations));

                context.currentStepStats.processedElems += associationJob.getProcessedCount();
            }

            updatePlayListPreferredArtworks(session, associations, true);
            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), processJobsDone };

            db::PlayListFileId lastRetrievedId{};
            db::IdRange<db::PlayListFileId> idRange;
            while (fetchNextPlayListIdRange(session, lastRetrievedId, idRange))
                queue.push(std::make_unique<ComputePlayListArtworkAssociationsJob>(_db, idRange));
        }

        updatePlayListPreferredArtworks(session, associations, false);
    }
} // namespace lms::scanner
