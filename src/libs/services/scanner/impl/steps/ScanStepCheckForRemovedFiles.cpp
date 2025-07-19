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

#include "ScanStepCheckForRemovedFiles.hpp"

#include <deque>
#include <filesystem>
#include <span>
#include <vector>

#include "core/IJob.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackLyrics.hpp"

#include "FileScanners.hpp"
#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"
#include "services/scanner/ScannerStats.hpp"

namespace lms::scanner
{
    namespace
    {
        template<typename IdType>
        struct FileToCheck
        {
            IdType objectId;
            std::filesystem::path file;
        };

        template<typename IdType>
        class CheckForRemovedFilesJob : public core::IJob
        {
        public:
            CheckForRemovedFilesJob(const ScannerSettings& settings, const FileScanners& scanners, std::span<const FileToCheck<IdType>> filesToCheck)
                : _settings{ settings }
                , _scanners{ scanners }
                , _filesToCheck{ std::cbegin(filesToCheck), std::cend(filesToCheck) }
            {
            }

            std::size_t getProcessedCount() const { return _processedCount; }
            std::span<const IdType> getObjectsToRemove() const { return _objectsToRemove; }

        private:
            core::LiteralString getName() const override { return "Check For Removed Files"; }
            void run() override
            {
                for (const FileToCheck<IdType>& fileToCheck : _filesToCheck)
                {
                    if (!checkFile(fileToCheck.file))
                        _objectsToRemove.push_back(fileToCheck.objectId);
                }

                _processedCount += _filesToCheck.size();
            }

            bool checkFile(const std::filesystem::path& p)
            {
                std::error_code ec;
                const std::filesystem::directory_entry fileEntry{ p, ec };
                if (ec)
                {
                    // TODO store error?
                    LMS_LOG(DBUPDATER, ERROR, "Error while checking file " << p << ": " << ec.message());
                    return false;
                }

                // For each track, make sure the the file still exists
                // and still belongs to a media directory
                if (!fileEntry.exists() || !fileEntry.is_regular_file())
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Removing " << p << ": missing");
                    return false;
                }

                if (std::none_of(std::cbegin(_settings.mediaLibraries), std::cend(_settings.mediaLibraries),
                        [&](const MediaLibraryInfo& libraryInfo) {
                            return core::pathUtils::isPathInRootPath(p, libraryInfo.rootDirectory, &excludeDirFileName);
                        }))
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Removing " << p << ": out of media directory");
                    return false;
                }

                if (!_scanners.select(p))
                {
                    LMS_LOG(DBUPDATER, DEBUG, "Removing " << p << ": file format no longer handled");
                    return false;
                }

                return true;
            }

            const ScannerSettings& _settings;
            const FileScanners& _scanners;
            std::vector<FileToCheck<IdType>> _filesToCheck;
            std::vector<IdType> _objectsToRemove;
            std::size_t _processedCount{};
        };

        template<typename Object>
        std::size_t removeObjects(db::Session& session, std::deque<typename Object::IdType>& objectIdsToRemove, bool forceFullBatch)
        {
            std::size_t removedObjectCount{};
            constexpr std::size_t writeBatchSize{ 50 };

            std::vector<typename Object::IdType> ids;
            while ((forceFullBatch && objectIdsToRemove.size() >= writeBatchSize) || !objectIdsToRemove.empty())
            {
                for (std::size_t i{}; !objectIdsToRemove.empty() && i < writeBatchSize; ++i)
                {
                    ids.push_back(objectIdsToRemove.front());
                    objectIdsToRemove.pop_front();
                }

                {
                    auto transaction{ session.createWriteTransaction() };
                    session.destroy<Object>(ids);
                }

                removedObjectCount += ids.size();
                ids.clear();
            }

            return removedObjectCount;
        }

        template<typename Object>
        bool fetchNextFilesToCheck(db::Session& session, typename Object::IdType& lastCheckedId, std::vector<FileToCheck<typename Object::IdType>>& filesToCheck)
        {
            constexpr std::size_t batchSize{ 200 };

            filesToCheck.clear();
            filesToCheck.reserve(batchSize);

            auto transaction{ session.createReadTransaction() };

            while (filesToCheck.size() < batchSize)
            {
                const typename Object::IdType previousLastCheckedId{ lastCheckedId };
                Object::findAbsoluteFilePath(session, lastCheckedId, batchSize, [&](Object::IdType objectId, const std::filesystem::path& filePath) {
                    // special case for track lyrics, only check external lyrics
                    if constexpr (std::is_same_v<Object, db::TrackLyrics>)
                    {
                        if (filePath.empty())
                            return;
                    }

                    filesToCheck.emplace_back(objectId, filePath);
                });

                if (previousLastCheckedId == lastCheckedId)
                    break;
            }

            return !filesToCheck.empty();
        }
    } // namespace

    bool ScanStepCheckForRemovedFiles::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // always check for removed files
        return true;
    }

    void ScanStepCheckForRemovedFiles::process(ScanContext& context)
    {
        db::Session& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = session.getFileStats().getTotalFileCount();
        }
        LMS_LOG(DBUPDATER, DEBUG, context.currentStepStats.totalElems << " files to be checked...");

        checkForRemovedFiles<db::Track>(context);
        checkForRemovedFiles<db::Image>(context);
        checkForRemovedFiles<db::TrackLyrics>(context);
        checkForRemovedFiles<db::PlayListFile>(context);
        checkForRemovedFiles<db::ArtistInfo>(context);
    }

    template<typename Object>
    void ScanStepCheckForRemovedFiles::checkForRemovedFiles(ScanContext& context)
    {
        using ObjectIdType = typename Object::IdType;

        if (_abortScan)
            return;

        db::Session& session{ _db.getTLSSession() };

        std::deque<ObjectIdType> objectIdsToRemove;

        auto processJobsDone = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& checkJob{ static_cast<const CheckForRemovedFilesJob<ObjectIdType>&>(*job) };
                std::span<const ObjectIdType> objectsToRemove{ checkJob.getObjectsToRemove() };

                objectIdsToRemove.insert(std::end(objectIdsToRemove), std::cbegin(objectsToRemove), std::cend(objectsToRemove));

                context.currentStepStats.processedElems += checkJob.getProcessedCount();
            }

            if (!objectIdsToRemove.empty())
                context.stats.deletions += removeObjects<Object>(session, objectIdsToRemove, true);

            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), 50, processJobsDone, 1, 0.85F };

            ObjectIdType lastCheckedId;
            std::vector<FileToCheck<ObjectIdType>> filesToCheck;
            while (fetchNextFilesToCheck<Object>(session, lastCheckedId, filesToCheck))
                queue.push(std::make_unique<CheckForRemovedFilesJob<ObjectIdType>>(_settings, getFileScanners(), filesToCheck));
        }

        // process all remaining objects
        context.stats.deletions += removeObjects<Object>(session, objectIdsToRemove, false);
    }

} // namespace lms::scanner
