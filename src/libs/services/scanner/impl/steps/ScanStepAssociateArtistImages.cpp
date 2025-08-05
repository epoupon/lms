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

#include "ScanStepAssociateArtistImages.hpp"

#include <array>
#include <cassert>
#include <deque>
#include <set>
#include <span>

#include "core/IConfig.hpp"
#include "core/IJob.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "core/String.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"

namespace lms::scanner
{
    namespace
    {
        struct ArtistArtworkAssociation
        {
            db::ArtistId artistId;
            db::ArtworkId preferredArtworkId;
        };
        using ArtistArtworkAssociationContainer = std::deque<ArtistArtworkAssociation>;

        struct SearchArtistArtworkParams
        {
            std::span<const std::string> artistFileNames;
            const ScannerSettings& settings;
        };

        db::Image::pointer findImageInDirectory(db::Session& session, const std::filesystem::path& directoryPath, std::span<const std::string> fileStemsToSearch)
        {
            db::Image::pointer image;

            const db::Directory::pointer directory{ db::Directory::find(session, directoryPath) };
            if (directory) // may not exist for artists that are split on different media libraries
            {
                for (std::string_view fileStem : fileStemsToSearch)
                {
                    db::Image::FindParameters params;
                    params.setDirectory(directory->getId());
                    params.setFileStem(fileStem, db::Image::FindParameters::ProcessWildcards{ true }); // no need to sanitize here, user is responsible for providing sanitized file stems in conf file

                    db::Image::find(session, params, [&](const db::Image::pointer foundImg) {
                        if (!image)
                            image = foundImg;
                    });

                    if (image)
                        break;
                }
            }

            return image;
        }

        db::Image::pointer getImageFromMbid(db::Session& session, const core::UUID& mbid)
        {
            db::Image::pointer image;

            // Find anywhere, since it is supposed to be unique!
            db::Image::find(session, db::Image::FindParameters{}.setFileStem(mbid.getAsString()), [&](const db::Image::pointer foundImg) {
                if (!image)
                    image = foundImg;
            });

            return image;
        }

        db::Image::pointer searchImageInArtistInfoDirectory(db::Session& session, db::ArtistId artistId)
        {
            db::Image::pointer image;

            std::vector<std::string> fileInfoPaths;
            db::ArtistInfo::find(session, artistId, [&](const db::ArtistInfo::pointer& artistInfo) {
                fileInfoPaths.push_back(artistInfo->getAbsoluteFilePath());

                if (!image)
                    image = findImageInDirectory(session, artistInfo->getDirectory()->getAbsolutePath(), std::array<std::string, 2>{ "thumb", "folder" });
            });

            if (fileInfoPaths.size() > 1)
                LMS_LOG(DBUPDATER, DEBUG, "Found " << fileInfoPaths.size() << " artist info files for same artist: " << core::stringUtils::joinStrings(fileInfoPaths, ", "));

            return image;
        }

        db::Image::pointer searchImageInDirectories(db::Session& session, const SearchArtistArtworkParams& searchParams, db::ArtistId artistId)
        {
            db::Image::pointer image;

            std::set<std::filesystem::path> releasePaths;
            db::Directory::FindParameters params;
            params.setArtist(artistId, { db::TrackArtistLinkType::ReleaseArtist });

            db::Directory::find(session, params, [&](const db::Directory::pointer& directory) {
                releasePaths.insert(directory->getAbsolutePath());
            });

            if (!releasePaths.empty())
            {
                // Expect layout like this:
                // ReleaseArtist/Release/Tracks'
                //              /someUserConfiguredArtistFile.jpg
                //
                // Or:
                // ReleaseArtist/SomeGrouping/Release/Tracks'
                //              /someUserConfiguredArtistFile.jpg
                //
                std::filesystem::path directoryToInspect{ core::pathUtils::getLongestCommonPath(std::cbegin(releasePaths), std::cend(releasePaths)) };
                while (true)
                {
                    image = findImageInDirectory(session, directoryToInspect, searchParams.artistFileNames);
                    if (image)
                        return image;

                    std::filesystem::path parentPath{ directoryToInspect.parent_path() };
                    if (parentPath == directoryToInspect)
                        break;

                    directoryToInspect = parentPath;
                }

                // Expect layout like this:
                // ReleaseArtist/Release/Tracks'
                //                      /artist.jpg
                //                      /someOtherUserConfiguredArtistFile.jpg
                for (const std::filesystem::path& releasePath : releasePaths)
                {
                    image = findImageInDirectory(session, releasePath, searchParams.artistFileNames);
                    if (image)
                        return image;
                }
            }

            return image;
        }

        db::Artwork::pointer getFirstReleaseArtwork(db::Session& session, const db::Artist::pointer& artist)
        {
            db::Artwork::pointer artwork;

            db::Release::FindParameters params;
            params.setArtist(artist->getId(), { db::TrackArtistLinkType::ReleaseArtist });
            params.setSortMethod(db::ReleaseSortMethod::OriginalDate);

            db::Release::find(session, params, [&](const db::Release::pointer& release) {
                if (artwork)
                    return;

                artwork = release->getPreferredArtwork();
            });

            return artwork;
        }

        db::Artwork::pointer computePreferredArtistArtwork(db::Session& session, const SearchArtistArtworkParams& searchParams, const db::Artist::pointer& artist)
        {
            if (const auto mbid{ artist->getMBID() })
            {
                const db::Image::pointer image{ getImageFromMbid(session, *mbid) };
                if (image)
                    return db::Artwork::find(session, image->getId());
            }

            {
                const db::Image::pointer image{ searchImageInArtistInfoDirectory(session, artist->getId()) };
                if (image)
                    return db::Artwork::find(session, image->getId());
            }

            {
                const db::Image::pointer image{ searchImageInDirectories(session, searchParams, artist->getId()) };
                if (image)
                    return db::Artwork::find(session, image->getId());
            }

            if (searchParams.settings.artistImageFallbackToRelease)
            {
                const db::Artwork::pointer artwork{ getFirstReleaseArtwork(session, artist) };
                if (artwork)
                    return artwork;
            }

            return db::Artwork::pointer{};
        }

        void updateArtistPreferredArtwork(db::Session& session, const ArtistArtworkAssociation& artistArtworkAssociation)
        {
            db::Artist::updatePreferredArtwork(session, artistArtworkAssociation.artistId, artistArtworkAssociation.preferredArtworkId);
        }

        void updateArtistPreferredArtworks(db::Session& session, ArtistArtworkAssociationContainer& imageAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while ((forceFullBatch && imageAssociations.size() >= writeBatchSize) || !imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateArtistPreferredArtwork(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }

        std::vector<std::string> constructArtistFileNames()
        {
            std::vector<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("artist-image-file-names",
                [&res](std::string_view fileName) {
                    res.emplace_back(fileName);
                },
                { "artist" });

            return res;
        }

        bool fetchNextArtistIdRange(db::Session& session, db::ArtistId& lastRetrievedId, db::IdRange<db::ArtistId>& idRange)
        {
            constexpr std::size_t readBatchSize{ 100 };

            auto transaction{ session.createReadTransaction() };

            idRange = db::Artist::findNextIdRange(session, lastRetrievedId, readBatchSize);
            lastRetrievedId = idRange.last;

            return idRange.isValid();
        }

        class ComputeArtistArtworkAssociationsJob : public core::IJob
        {
        public:
            ComputeArtistArtworkAssociationsJob(db::IDb& db, const SearchArtistArtworkParams& searchParams, db::IdRange<db::ArtistId> artistIdRange)
                : _db{ db }
                , _searchParams{ searchParams }
                , _artistIdRange{ artistIdRange }
            {
            }

            std::span<const ArtistArtworkAssociation> getAssociations() const { return _associations; }
            std::size_t getProcessedArtistCount() const { return _processedArtistCount; }

        private:
            core::LiteralString getName() const override { return "Associate Artist Artworks"; }
            void run() override
            {
                auto& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::Artist::find(session, _artistIdRange, [this, &session](const db::Artist::pointer& artist) {
                    const db::Artwork::pointer preferredArtwork{ computePreferredArtistArtwork(session, _searchParams, artist) };

                    if (artist->getPreferredArtwork() != preferredArtwork)
                    {
                        _associations.push_back(ArtistArtworkAssociation{ artist->getId(), preferredArtwork ? preferredArtwork->getId() : db::ArtworkId{} });

                        if (preferredArtwork)
                            LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork for artist '" << artist->getName() << "' with image in " << preferredArtwork->getAbsoluteFilePath());
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Removing preferred artwork from artist '" << artist->getName() << "'");
                    }

                    _processedArtistCount++;
                });
            }

            db::IDb& _db;
            const SearchArtistArtworkParams& _searchParams;
            db::IdRange<db::ArtistId> _artistIdRange;
            std::vector<ArtistArtworkAssociation> _associations;
            std::size_t _processedArtistCount{};
        };

    } // namespace

    ScanStepAssociateArtistImages::ScanStepAssociateArtistImages(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _artistFileNames{ constructArtistFileNames() }
    {
    }

    bool ScanStepAssociateArtistImages::needProcess(const ScanContext& context) const
    {
        if (context.stats.getChangesCount() > 0)
            return true;

        if (getLastScanSettings() && getLastScanSettings()->artistImageFallbackToRelease != _settings.artistImageFallbackToRelease)
            return true;

        return false;
    }

    void ScanStepAssociateArtistImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Artist::getCount(session);
        }

        const SearchArtistArtworkParams searchParams{
            .artistFileNames = _artistFileNames,
            .settings = _settings,
        };

        ArtistArtworkAssociationContainer artistArtworkAssociations;
        auto processJobsDone = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& associationJob{ static_cast<const ComputeArtistArtworkAssociationsJob&>(*job) };
                const auto& artistAssociations{ associationJob.getAssociations() };

                artistArtworkAssociations.insert(std::end(artistArtworkAssociations), std::cbegin(artistAssociations), std::cend(artistAssociations));

                context.currentStepStats.processedElems += associationJob.getProcessedArtistCount();
            }

            updateArtistPreferredArtworks(session, artistArtworkAssociations, true);
            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), 20, processJobsDone, 1, 0.85F };

            db::ArtistId lastRetrievedArtistId{};
            db::IdRange<db::ArtistId> artistIdRange;
            while (fetchNextArtistIdRange(session, lastRetrievedArtistId, artistIdRange))
                queue.push(std::make_unique<ComputeArtistArtworkAssociationsJob>(_db, searchParams, artistIdRange));
        }

        // process all remaining associations
        updateArtistPreferredArtworks(session, artistArtworkAssociations, false);
    }
} // namespace lms::scanner
