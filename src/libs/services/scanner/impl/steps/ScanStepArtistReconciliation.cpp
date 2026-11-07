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

#include "ScanStepArtistReconciliation.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

#include "core/ILogger.hpp"

#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackList.hpp"

#include "ScanContext.hpp"
#include "ScannerSettings.hpp"
#include "helpers/ArtistHelpers.hpp"
#include "types/TrackMetadata.hpp"

namespace lms::scanner
{
    namespace
    {
        void recomputeArtist(db::ISession& session, db::TrackArtistLink::pointer link, bool allowArtistMBIDFallback)
        {
            assert(!link->isArtistMBIDMatched());

            Artist artistInfo{ std::nullopt, link->getArtistName(), link->getArtistSortName().empty() ? std::nullopt : std::make_optional<std::string>(link->getArtistSortName()) };

            db::Artist::pointer newArtist{ helpers::getOrCreateArtistByName(session, artistInfo, helpers::AllowFallbackOnMBIDEntry{ allowArtistMBIDFallback }) };
            LMS_LOG(DB, DEBUG, "Reconcile artist link for track " << link->getTrack()->getAbsoluteFilePath() << ", type " << static_cast<int>(link->getType()) << " from " << link->getArtist() << " to " << newArtist);

            assert(newArtist != link->getArtist());
            link.modify()->setArtist(newArtist);
        }

        void recomputeArtist(db::ISession& session, db::ArtistInfo::pointer artistInfo, bool allowArtistMBIDFallback)
        {
            assert(!artistInfo->isMBIDMatched());

            Artist artistMetadata{ std::nullopt, artistInfo->getName(), artistInfo->getSortName().empty() ? std::nullopt : std::make_optional<std::string>(artistInfo->getSortName()) };
            db::Artist::pointer newArtist{ helpers::getOrCreateArtistByName(session, artistMetadata, helpers::AllowFallbackOnMBIDEntry{ allowArtistMBIDFallback }) };
            LMS_LOG(DB, DEBUG, "Reconcile artist link for artist info " << artistInfo->getAbsoluteFilePath() << " from " << artistInfo->getArtist() << " to " << newArtist);

            assert(newArtist != artistInfo->getArtist());
            artistInfo.modify()->setArtist(newArtist);
        }
    } // namespace

    bool ScanStepArtistReconciliation::needProcess([[maybe_unused]] const ScanContext& context) const
    {
        // Since this step is very fast in case there is nothing to do, no need to skip if nothing has changed
        return true;
    }

    void ScanStepArtistReconciliation::process(ScanContext& context)
    {
        // Relinking below relies on up to date artist names
        updateArtistNames(context);

        // Reconcile artist links when MBID not used to match
        {
            // Order is important
            updateLinksForArtistNameNoLongerMatch(context);
            updateLinksWithArtistNameAmbiguity(context);
        }

        // Reconcile artist info when MBID not used to match
        {
            // Order is important
            updateArtistInfoForArtistNameNoLongerMatch(context);
            updateArtistInfoWithArtistNameAmbiguity(context);
        }

        // Relinking may have created artists or moved their sources
        updateArtistNames(context);
    }

    void ScanStepArtistReconciliation::updateArtistNames(ScanContext& context)
    {
        static constexpr std::size_t batchSize{ 50 };

        db::ISession& session{ _db.getTLSSession() };

        // The file scan only sets artist names/sort names on creation: this is the single place where they follow their sources
        struct ArtistToUpdate
        {
            db::Artist::pointer artist;
            std::string name;
            std::string sortName;
        };
        std::vector<ArtistToUpdate> artistsToUpdate;
        {
            auto transaction{ session.createReadTransaction() };

            db::Artist::findNameUpdates(session, [&](const db::Artist::pointer& artist, std::string_view newName, std::string_view newSortName) {
                artistsToUpdate.push_back(ArtistToUpdate{ .artist = artist, .name = std::string{ newName }, .sortName = std::string{ newSortName } });
            });
        }

        std::size_t index{};
        while (index < artistsToUpdate.size() && !_abortScan)
        {
            auto transaction{ session.createWriteTransaction() };

            for (const std::size_t batchEnd{ std::min(index + batchSize, artistsToUpdate.size()) }; index < batchEnd; ++index)
            {
                ArtistToUpdate& artistToUpdate{ artistsToUpdate[index] };
                LMS_LOG(DBUPDATER, DEBUG, "Updating artist " << artistToUpdate.artist << " name to '" << artistToUpdate.name << "', sort name to '" << artistToUpdate.sortName << "'");
                artistToUpdate.artist.modify()->setName(artistToUpdate.name);
                artistToUpdate.artist.modify()->setSortName(artistToUpdate.sortName);
            }
        }

        _progressCallback(context.currentStepStats);
    }

    void ScanStepArtistReconciliation::updateArtistInfoForArtistNameNoLongerMatch(ScanContext& context)
    {
        static constexpr std::size_t batchSize{ 50 };

        const bool allowArtistMBIDFallback{ _settings.allowArtistMBIDFallback };
        db::ISession& session{ _db.getTLSSession() };

        std::vector<db::ArtistInfo::pointer> artistInfo;
        while (!_abortScan)
        {
            artistInfo.clear();
            {
                auto transaction{ session.createReadTransaction() };
                db::ArtistInfo::findArtistNameNoLongerMatch(session, db::Range{ .offset = 0, .size = batchSize }, [&](const db::ArtistInfo::pointer& link) {
                    artistInfo.push_back(link);
                });
            }

            if (artistInfo.empty())
                break;

            {
                auto transaction{ session.createWriteTransaction() };
                for (db::ArtistInfo::pointer& info : artistInfo)
                {
                    recomputeArtist(session, info, allowArtistMBIDFallback);
                    context.currentStepStats.processedElems++;
                }

                _progressCallback(context.currentStepStats);
            }
        }
    }

    void ScanStepArtistReconciliation::updateArtistInfoWithArtistNameAmbiguity(ScanContext& context)
    {
        static constexpr std::size_t batchSize{ 50 };

        const bool allowArtistMBIDFallback{ _settings.allowArtistMBIDFallback };
        db::ISession& session{ _db.getTLSSession() };

        std::vector<db::ArtistInfo::pointer> artistInfo;
        while (!_abortScan)
        {
            artistInfo.clear();
            {
                auto transaction{ session.createReadTransaction() };
                db::ArtistInfo::findWithArtistNameAmbiguity(session, db::Range{ .offset = 0, .size = batchSize }, allowArtistMBIDFallback, [&](const db::ArtistInfo::pointer& info) {
                    artistInfo.push_back(info);
                });
            }

            if (artistInfo.empty())
                break;

            {
                auto transaction{ session.createWriteTransaction() };
                for (db::ArtistInfo::pointer& info : artistInfo)
                {
                    recomputeArtist(session, info, allowArtistMBIDFallback);
                    context.currentStepStats.processedElems++;
                }

                _progressCallback(context.currentStepStats);
            }
        }
    }
    void ScanStepArtistReconciliation::updateLinksForArtistNameNoLongerMatch(ScanContext& context)
    {
        static constexpr std::size_t batchSize{ 50 };

        const bool allowArtistMBIDFallback{ _settings.allowArtistMBIDFallback };
        db::ISession& session{ _db.getTLSSession() };

        std::vector<db::TrackArtistLink::pointer> links;
        while (!_abortScan)
        {
            links.clear();
            {
                auto transaction{ session.createReadTransaction() };
                db::TrackArtistLink::findArtistNameNoLongerMatch(session, db::Range{ .offset = 0, .size = batchSize }, [&](const db::TrackArtistLink::pointer& link) {
                    links.push_back(link);
                });
            }

            if (links.empty())
                break;

            {
                auto transaction{ session.createWriteTransaction() };
                for (db::TrackArtistLink::pointer& link : links)
                {
                    recomputeArtist(session, link, allowArtistMBIDFallback);
                    context.currentStepStats.processedElems++;
                }

                _progressCallback(context.currentStepStats);
            }
        }
    }

    void ScanStepArtistReconciliation::updateLinksWithArtistNameAmbiguity(ScanContext& context)
    {
        static constexpr std::size_t batchSize{ 50 };

        const bool allowArtistMBIDFallback{ _settings.allowArtistMBIDFallback };
        db::ISession& session{ _db.getTLSSession() };

        std::vector<db::TrackArtistLink::pointer> links;
        while (!_abortScan)
        {
            links.clear();
            {
                auto transaction{ session.createReadTransaction() };
                db::TrackArtistLink::findWithArtistNameAmbiguity(session, db::Range{ .offset = 0, .size = batchSize }, allowArtistMBIDFallback, [&](const db::TrackArtistLink::pointer& link) {
                    links.push_back(link);
                });
            }

            if (links.empty())
                break;

            {
                auto transaction{ session.createWriteTransaction() };
                for (db::TrackArtistLink::pointer& link : links)
                {
                    recomputeArtist(session, link, allowArtistMBIDFallback);
                    context.currentStepStats.processedElems++;
                }

                _progressCallback(context.currentStepStats);
            }
        }
    }
} // namespace lms::scanner
