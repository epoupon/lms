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

#include "services/database/Artist.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "services/database/TrackBookmark.hpp"
#include "services/database/TrackFeatures.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/Types.hpp"
#include "services/database/User.hpp"

template <typename T>
class ScopedEntity
{
	public:
		using IdType = typename T::IdType;

		template <typename... Args>
		ScopedEntity(Database::Session& session, Args&& ...args)
			: _session {session}
		{
			auto transaction {_session.createUniqueTransaction()};

			auto entity {_session.create<T>(std::forward<Args>(args)...)};
			EXPECT_TRUE(entity);
			_id = entity->getId();
		}

		~ScopedEntity()
		{
			auto transaction {_session.createUniqueTransaction()};

			auto entity {T::find(_session, _id)};
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
			auto transaction {_session.createSharedTransaction()};
			return get();
		}

		typename T::pointer get()
		{
			_session.checkSharedLocked();

			auto entity {T::find(_session, _id)};
			EXPECT_TRUE(entity);
			return entity;
		}

		typename T::pointer operator->()
		{
			return get();
		}

		IdType getId() const { return _id; }

	private:
		Database::Session& _session;
		IdType _id {};
};

using ScopedArtist = ScopedEntity<Database::Artist>;
using ScopedCluster = ScopedEntity<Database::Cluster>;
using ScopedClusterType = ScopedEntity<Database::ClusterType>;
using ScopedRelease = ScopedEntity<Database::Release>;
using ScopedTrack = ScopedEntity<Database::Track>;
using ScopedTrackList = ScopedEntity<Database::TrackList>;
using ScopedUser = ScopedEntity<Database::User>;

class ScopedFileDeleter final
{
	public:
		ScopedFileDeleter(const std::filesystem::path& path) : _path {path} {}
		~ScopedFileDeleter() { std::filesystem::remove(_path); }

		ScopedFileDeleter(const ScopedFileDeleter&) = delete;
		ScopedFileDeleter(ScopedFileDeleter&&) = delete;
		ScopedFileDeleter operator=(const ScopedFileDeleter&) = delete;
		ScopedFileDeleter operator=(ScopedFileDeleter&&) = delete;

	private:
		const std::filesystem::path _path;
};

class TmpDatabase final
{
	public:
		TmpDatabase ();

		Database::Db& getDb();

	private:
		const std::filesystem::path _tmpFile;
		ScopedFileDeleter _fileDeleter;
		Database::Db _db;
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

	static inline std::unique_ptr<TmpDatabase> _tmpDb {};

public:
	Database::Session session {_tmpDb->getDb()};
};

