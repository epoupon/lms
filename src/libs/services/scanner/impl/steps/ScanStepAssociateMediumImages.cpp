/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ScanStepAssociateMediumImages.hpp"

#include <array>
#include <cassert>
#include <deque>
#include <set>
#include <span>

#include "core/IConfig.hpp"
#include "core/IJob.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"

#include "JobQueue.hpp"
#include "ScanContext.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"

namespace lms::scanner
{
    namespace
    {
        struct MediumArtworkAssociation
        {
            db::MediumId mediumId;
            db::ArtworkId preferredArtworkId;
        };
        using MediumArtworkAssociationContainer = std::deque<MediumArtworkAssociation>;

        struct SearchMediumArtworkParams
        {
            std::span<const std::string_view> mediumFileNames;
        };

        db::Image::pointer findImageInDirectory(db::Session& session, const db::Directory::pointer& directory, std::span<const std::string_view> fileStemsToSearch, db::Image::FindParameters::ProcessWildcards processWildcards)
        {
            db::Image::pointer image;

            for (std::string_view fileStem : fileStemsToSearch)
            {
                db::Image::FindParameters params;
                params.setDirectory(directory->getId());
                params.setFileStem(fileStem, processWildcards);

                db::Image::find(session, params, [&](const db::Image::pointer foundImg) {
                    if (!image)
                        image = foundImg;
                });

                if (image)
                    break;
            }
            return image;
        }

        db::Image::pointer searchImageInDirectories(db::Session& session, const SearchMediumArtworkParams& searchParams, const db::Medium::pointer& medium)
        {
            db::Image::pointer image;

            std::set<std::filesystem::path> mediumPaths;
            db::Directory::FindParameters params;
            params.setMedium(medium->getId());

            // Expect layout like this:
            // Release/Tracks
            //        /NameOfTheDisc.jpg
            //        /someOtherUserConfiguredMediumFile.jpg
            //
            // Or:
            // Release/CD X/Tracks'
            //             /NameOfDisc.jpg.jpg
            //             /someOtherUserConfiguredMediumFile.jpg
            //
            // We don't expect mediums to be split across multiple directories, so we can just search for the first directory that matches the medium.
            db::Directory::find(session, params, [&](const db::Directory::pointer& directory) {
                if (image)
                    return;

                if (const std::string mediumName{ core::pathUtils::sanitizeFileStem(medium->getName()) }; !mediumName.empty())
                {
                    std::string_view mediumNameView{ mediumName };
                    image = findImageInDirectory(session, directory, std::span{ &mediumNameView, 1 }, db::Image::FindParameters::ProcessWildcards{ false });
                }

                if (!image)
                    image = findImageInDirectory(session, directory, searchParams.mediumFileNames, db::Image::FindParameters::ProcessWildcards{ true });
            });

            return image;
        }

        db::TrackEmbeddedImage::pointer getArtworkFromTracks(db::Session& session, const db::Medium::pointer& medium)
        {
            db::TrackEmbeddedImage::pointer image;

            db::TrackEmbeddedImage::FindParameters params;
            params.setMedium(medium->getId());
            params.setImageType(db::ImageType::Media);
            params.setSortMethod(db::TrackEmbeddedImageSortMethod::TrackNumberThenSizeDesc);

            db::TrackEmbeddedImage::find(session, params, [&](const db::TrackEmbeddedImage::pointer& foundImage) {
                if (image)
                    return;

                image = foundImage;
            });

            return image;
        }

        db::Artwork::pointer computePreferredMediumArtwork(db::Session& session, const SearchMediumArtworkParams& searchParams, const db::Medium::pointer& medium)
        {
            if (const db::Image::pointer image{ searchImageInDirectories(session, searchParams, medium) })
                return db::Artwork::find(session, image->getId());

            if (const db::TrackEmbeddedImage::pointer image{ getArtworkFromTracks(session, medium) })
                return db::Artwork::find(session, image->getId());

            return db::Artwork::pointer{};
        }

        void updateMediumPreferredArtwork(db::Session& session, const MediumArtworkAssociation& mediumArtworkAssociation)
        {
            db::Medium::updatePreferredArtwork(session, mediumArtworkAssociation.mediumId, mediumArtworkAssociation.preferredArtworkId);
        }

        void updateMediumPreferredArtworks(db::Session& session, MediumArtworkAssociationContainer& imageAssociations, bool forceFullBatch)
        {
            constexpr std::size_t writeBatchSize{ 50 };

            while ((forceFullBatch && imageAssociations.size() >= writeBatchSize) || !imageAssociations.empty())
            {
                auto transaction{ session.createWriteTransaction() };

                for (std::size_t i{}; !imageAssociations.empty() && i < writeBatchSize; ++i)
                {
                    updateMediumPreferredArtwork(session, imageAssociations.front());
                    imageAssociations.pop_front();
                }
            }
        }

        std::vector<std::string> constructMediumFileNames()
        {
            std::vector<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("medium-image-file-names",
                [&res](std::string_view fileName) {
                    res.emplace_back(fileName);
                },
                { "discsubtitle" });

            return res;
        }

        bool fetchNextMediumIdRange(db::Session& session, db::MediumId& lastRetrievedId, db::IdRange<db::MediumId>& idRange)
        {
            constexpr std::size_t readBatchSize{ 100 };

            auto transaction{ session.createReadTransaction() };

            idRange = db::Medium::findNextIdRange(session, lastRetrievedId, readBatchSize);
            lastRetrievedId = idRange.last;

            return idRange.isValid();
        }

        class ComputeMediumArtworkAssociationsJob : public core::IJob
        {
        public:
            ComputeMediumArtworkAssociationsJob(db::IDb& db, const SearchMediumArtworkParams& searchParams, db::IdRange<db::MediumId> mediumIdRange)
                : _db{ db }
                , _searchParams{ searchParams }
                , _mediumIdRange{ mediumIdRange }
            {
            }
            ~ComputeMediumArtworkAssociationsJob() override = default;

            ComputeMediumArtworkAssociationsJob(const ComputeMediumArtworkAssociationsJob&) = delete;
            ComputeMediumArtworkAssociationsJob& operator=(const ComputeMediumArtworkAssociationsJob&) = delete;

            std::span<const MediumArtworkAssociation> getAssociations() const { return _associations; }
            std::size_t getProcessedMediumCount() const { return _processedMediumCount; }

        private:
            core::LiteralString getName() const override { return "Associate Medium Artworks"; }
            void run() override
            {
                auto& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::Medium::find(session, _mediumIdRange, [this, &session](const db::Medium::pointer& medium) {
                    const db::Artwork::pointer preferredArtwork{ computePreferredMediumArtwork(session, _searchParams, medium) };

                    if (medium->getPreferredArtwork() != preferredArtwork)
                    {
                        _associations.push_back(MediumArtworkAssociation{ medium->getId(), preferredArtwork ? preferredArtwork->getId() : db::ArtworkId{} });

                        if (preferredArtwork)
                            LMS_LOG(DBUPDATER, DEBUG, "Updating preferred artwork for medium '" << medium->getName() << "(from '" << medium->getRelease()->getName() << "') with image in " << preferredArtwork->getAbsoluteFilePath());
                        else
                            LMS_LOG(DBUPDATER, DEBUG, "Removing preferred artwork from medium '" << medium->getName() << "(from '" << medium->getRelease()->getName() << "')");
                    }

                    _processedMediumCount++;
                });
            }

            db::IDb& _db;
            const SearchMediumArtworkParams& _searchParams;
            db::IdRange<db::MediumId> _mediumIdRange;
            std::vector<MediumArtworkAssociation> _associations;
            std::size_t _processedMediumCount{};
        };

    } // namespace

    ScanStepAssociateMediumImages::ScanStepAssociateMediumImages(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _mediumFileNames{ constructMediumFileNames() }
    {
    }

    bool ScanStepAssociateMediumImages::needProcess(const ScanContext& context) const
    {
        return context.stats.getChangesCount() > 0;
    }

    void ScanStepAssociateMediumImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Artist::getCount(session);
        }

        std::vector<std::string_view> mediumFileNames;
        mediumFileNames.reserve(_mediumFileNames.size());
        for (const std::string& fileName : _mediumFileNames)
            mediumFileNames.push_back(fileName);

        const SearchMediumArtworkParams searchParams{
            .mediumFileNames = mediumFileNames,
        };

        MediumArtworkAssociationContainer mediumArtworkAssociations;
        auto processJobsDone = [&](std::span<std::unique_ptr<core::IJob>> jobs) {
            if (_abortScan)
                return;

            for (const auto& job : jobs)
            {
                const auto& associationJob{ static_cast<const ComputeMediumArtworkAssociationsJob&>(*job) };
                const auto& artistAssociations{ associationJob.getAssociations() };

                mediumArtworkAssociations.insert(std::end(mediumArtworkAssociations), std::cbegin(artistAssociations), std::cend(artistAssociations));

                context.currentStepStats.processedElems += associationJob.getProcessedMediumCount();
            }

            updateMediumPreferredArtworks(session, mediumArtworkAssociations, true);
            _progressCallback(context.currentStepStats);
        };

        {
            JobQueue queue{ getJobScheduler(), 20, processJobsDone, 1, 0.85F };

            db::MediumId lastRetrievedMediumId{};
            db::IdRange<db::MediumId> mediumIdRange;
            while (fetchNextMediumIdRange(session, lastRetrievedMediumId, mediumIdRange))
                queue.push(std::make_unique<ComputeMediumArtworkAssociationsJob>(_db, searchParams, mediumIdRange));
        }

        // process all remaining associations
        updateMediumPreferredArtworks(session, mediumArtworkAssociations, false);
    }
} // namespace lms::scanner
