/*
 * Copyright (C) 2021 Emeric Poupon
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

#pragma once

#include <filesystem>
#include <memory>

#include <gtest/gtest.h>

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackBookmark.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"

namespace lms::db::tests
{
    template<typename T>
    class [[nodiscard]] ScopedEntity
    {
    public:
        using IdType = typename T::IdType;

        template<typename... Args>
        ScopedEntity(db::Session& session, Args&&... args)
            : _session{ session }
        {
            auto transaction{ _session.createWriteTransaction() };

            auto entity{ _session.create<T>(std::forward<Args>(args)...) };
            EXPECT_TRUE(entity);
            _id = entity->getId();
        }

        ~ScopedEntity()
        {
            auto transaction{ _session.createWriteTransaction() };

            auto entity{ T::find(_session, _id) };
            // could not be here due to "on delete cascade" constraints...
            if (entity)
                entity.remove();
        }

        ScopedEntity(const ScopedEntity&) = delete;
        ScopedEntity(ScopedEntity&&) = delete;
        ScopedEntity& operator=(const ScopedEntity&) = delete;
        ScopedEntity& operator=(ScopedEntity&&) = delete;

        typename T::pointer lockAndGet()
        {
            auto transaction{ _session.createReadTransaction() };
            return get();
        }

        typename T::pointer get()
        {
            _session.checkReadTransaction();

            auto entity{ T::find(_session, _id) };
            EXPECT_TRUE(entity);
            return entity;
        }

        typename T::pointer operator->()
        {
            return get();
        }

        IdType getId() const { return _id; }

    private:
        db::Session& _session;
        IdType _id{};
    };

    using ScopedArtist = ScopedEntity<db::Artist>;
    using ScopedCluster = ScopedEntity<db::Cluster>;
    using ScopedClusterType = ScopedEntity<db::ClusterType>;
    using ScopedMediaLibrary = ScopedEntity<db::MediaLibrary>;
    using ScopedRelease = ScopedEntity<db::Release>;
    using ScopedTrack = ScopedEntity<db::Track>;
    using ScopedTrackList = ScopedEntity<db::TrackList>;
    using ScopedUser = ScopedEntity<db::User>;

    class ScopedFileDeleter final
    {
    public:
        ScopedFileDeleter(const std::filesystem::path& path)
            : _path{ path } {}
        ~ScopedFileDeleter() { std::filesystem::remove(_path); }

    private:
        ScopedFileDeleter(const ScopedFileDeleter&) = delete;
        ScopedFileDeleter(ScopedFileDeleter&&) = delete;
        ScopedFileDeleter operator=(const ScopedFileDeleter&) = delete;
        ScopedFileDeleter operator=(ScopedFileDeleter&&) = delete;

        const std::filesystem::path _path;
    };

    class TmpDatabase final
    {
    public:
        TmpDatabase();

        IDb& getDb();

    private:
        const std::filesystem::path _tmpFile;
        ScopedFileDeleter _fileDeleter;
        std::unique_ptr<IDb> _db;
    };

    class DatabaseFixture : public ::testing::Test
    {
    public:
        ~DatabaseFixture();

    public:
        static void SetUpTestCase();
        static void TearDownTestCase();

    private:
        void testDatabaseEmpty();

        static inline std::unique_ptr<TmpDatabase> _tmpDb{};

    public:
        db::Session session{ _tmpDb->getDb() };
    };
} // namespace lms::db::tests