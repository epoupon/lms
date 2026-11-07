/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "core/IJobScheduler.hpp"
#include "core/PartialDateTime.hpp"
#include "core/ScopedTmpDirectory.hpp"
#include "database/IDb.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"

#include "TmpDatabase.hpp"

#include "FileScanners.hpp"
#include "ScanContext.hpp"
#include "ScannerSettings.hpp"
#include "scanners/FileToScan.hpp"
#include "scanners/IFileScanOperation.hpp"
#include "scanners/artistinfo/ArtistInfoFileScanner.hpp"
#include "steps/ScanStepArtistReconciliation.hpp"

namespace lms::scanner::tests
{
    namespace
    {
        const std::optional<core::UUID> artistMBID{ core::UUID::fromString("c2a2f396-5d62-4d8a-8395-b4ab9962f1b6") };

        class ArtistNames : public ::testing::Test
        {
        public:
            static void SetUpTestSuite()
            {
                _tmpDb = std::make_unique<db::tests::TmpDatabase>();
                _tmpDb->createTablesAndIndexes();
            }

            static void TearDownTestSuite()
            {
                _tmpDb.reset();
            }

        protected:
            void TearDown() override
            {
                {
                    auto transaction{ session.createWriteTransaction() };
                    db::tests::executeStatement(session, "DELETE FROM track_artist_link");
                    db::tests::executeStatement(session, "DELETE FROM track");
                    db::tests::executeStatement(session, "DELETE FROM artist_info");
                    db::tests::executeStatement(session, "DELETE FROM directory");
                    db::tests::executeStatement(session, "DELETE FROM media_library");
                    db::tests::executeStatement(session, "DELETE FROM artist");
                }

                auto transaction{ session.createReadTransaction() };
                EXPECT_TRUE(db::tests::areAllTablesEmpty(session));
            }

            db::ArtistId createArtist(std::string_view name, const std::optional<core::UUID>& mbid, std::string_view sortName)
            {
                auto transaction{ session.createWriteTransaction() };

                db::Artist::pointer artist{ db::Artist::create(session, std::string{ name }, mbid) };
                artist.modify()->setSortName(sortName);
                return artist->getId();
            }

            void createTrackArtistLink(db::ArtistId artistId, int year, bool mbidMatched, std::string_view name, std::string_view sortName)
            {
                auto transaction{ session.createWriteTransaction() };

                db::Track::pointer track{ db::Track::create(session) };
                track.modify()->setDate(core::PartialDateTime{ year });

                db::TrackArtistLink::pointer link{ db::TrackArtistLink::create(session, track, db::Artist::find(session, artistId), db::TrackArtistLinkType::Artist, mbidMatched) };
                link.modify()->setArtistName(name);
                link.modify()->setArtistSortName(sortName);
            }

            std::string getArtistName(db::ArtistId artistId)
            {
                auto transaction{ session.createReadTransaction() };
                return db::Artist::find(session, artistId)->getName();
            }

            std::string getArtistSortName(db::ArtistId artistId)
            {
                auto transaction{ session.createReadTransaction() };
                return db::Artist::find(session, artistId)->getSortName();
            }

            void scanArtistInfoFile(std::string_view content)
            {
                const std::filesystem::path filePath{ _rootDirectory.getPath() / "Artist" / "artist.nfo" };
                std::filesystem::create_directories(filePath.parent_path());
                {
                    std::ofstream ofs{ filePath };
                    ofs << content;
                }

                db::MediaLibraryId mediaLibraryId;
                {
                    auto transaction{ session.createWriteTransaction() };

                    db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(session, _rootDirectory.getPath()) };
                    if (!mediaLibrary)
                        mediaLibrary = db::MediaLibrary::create(session, "Test", _rootDirectory.getPath());
                    mediaLibraryId = mediaLibrary->getId();
                }

                const ScannerSettings settings;
                const ArtistInfoFileScanner scanner{ _tmpDb->getDb(), settings };
                const std::unique_ptr<IFileScanOperation> scanOperation{ static_cast<const IFileScanner&>(scanner).createScanOperation(FileToScan{ .filePath = filePath, .mediaLibrary = MediaLibraryInfo{ .id = mediaLibraryId, .rootDirectory = _rootDirectory.getPath() }, .lastWriteTime = {}, .fileSize = 0 }) };
                scanOperation->scan();
                ASSERT_TRUE(scanOperation->getErrors().empty());

                auto transaction{ session.createWriteTransaction() };
                scanOperation->processResult();
            }

            void reconcileArtists()
            {
                const ScannerSettings settings;
                std::atomic<bool> abortScan{};
                const FileScanners fileScanners;
                const std::filesystem::path cachePath;
                const std::unique_ptr<core::IJobScheduler> jobScheduler{ core::createJobScheduler("Scanner", 1) };

                ScanStepBase::InitParams initParams{
                    .jobScheduler = *jobScheduler,
                    .settings = settings,
                    .lastScanSettings = nullptr,
                    .progressCallback = [](const ScanStepStats&) {},
                    .abortScan = abortScan,
                    .db = _tmpDb->getDb(),
                    .fileScanners = fileScanners,
                    .cachePath = cachePath,
                };
                ScanStepArtistReconciliation step{ initParams };

                ScanContext context;
                static_cast<IScanStep&>(step).process(context);
            }

        private:
            static inline std::unique_ptr<db::tests::TmpDatabase> _tmpDb;
            const core::ScopedTmpDirectory _rootDirectory{ "lms-scanner-test-" };

        protected:
            db::ISession& session{ _tmpDb->getDb().getTLSSession() };
        };
    } // namespace

    TEST_F(ArtistNames, artistInfoFileWithoutSortName)
    {
        const db::ArtistId artistId{ createArtist("Artist", artistMBID, "") };
        createTrackArtistLink(artistId, 2000, true, "Artist", "Artist, Tag");

        scanArtistInfoFile(R"(<?xml version='1.0' encoding='UTF-8' standalone='yes'?>
<artist>
  <name>Artist</name>
  <musicBrainzArtistID>c2a2f396-5d62-4d8a-8395-b4ab9962f1b6</musicBrainzArtistID>
</artist>)");
        reconcileArtists();

        EXPECT_EQ(getArtistSortName(artistId), "Artist, Tag");
    }

    TEST_F(ArtistNames, artistInfoFileSortNameRemoved)
    {
        const db::ArtistId artistId{ createArtist("Artist", artistMBID, "") };
        createTrackArtistLink(artistId, 2000, true, "Artist", "Artist, Tag");

        scanArtistInfoFile(R"(<?xml version='1.0' encoding='UTF-8' standalone='yes'?>
<artist>
  <name>Artist</name>
  <sortname>Artist, Info</sortname>
  <musicBrainzArtistID>c2a2f396-5d62-4d8a-8395-b4ab9962f1b6</musicBrainzArtistID>
</artist>)");
        reconcileArtists();
        ASSERT_EQ(getArtistSortName(artistId), "Artist, Info");

        scanArtistInfoFile(R"(<?xml version='1.0' encoding='UTF-8' standalone='yes'?>
<artist>
  <name>Artist</name>
  <musicBrainzArtistID>c2a2f396-5d62-4d8a-8395-b4ab9962f1b6</musicBrainzArtistID>
</artist>)");
        reconcileArtists();

        EXPECT_EQ(getArtistSortName(artistId), "Artist, Tag");
    }

    TEST_F(ArtistNames, artistCreatedByRelinkingFollowsItsSources)
    {
        const db::ArtistId artistId{ createArtist("Old Artist", artistMBID, "Artist, Old") };
        createTrackArtistLink(artistId, 2010, true, "New Artist", "Artist, New");
        // merged links: they no longer match once the artist is renamed, and get relinked to a new artist created using the first of them
        createTrackArtistLink(artistId, 2000, false, "Old Artist", "Artist, Older Tag");
        createTrackArtistLink(artistId, 2005, false, "Old Artist", "Artist, Recent Tag");

        reconcileArtists();

        EXPECT_EQ(getArtistName(artistId), "New Artist");

        auto transaction{ session.createReadTransaction() };
        const std::vector<db::Artist::pointer> oldArtists{ db::Artist::find(session, "Old Artist") };
        ASSERT_EQ(oldArtists.size(), 1);
        EXPECT_FALSE(oldArtists[0]->hasMBID());
        EXPECT_EQ(oldArtists[0]->getSortName(), "Artist, Recent Tag");
    }
} // namespace lms::scanner::tests
