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
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "services/database/TrackBookmark.hpp"
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

			auto entity {T::create(_session, std::forward<Args>(args)...)};
			EXPECT_TRUE(entity);
			_id = entity->getId();
		}

		~ScopedEntity()
		{
			auto transaction {_session.createUniqueTransaction()};

			auto entity {T::getById(_session, _id)};
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

			auto entity {T::getById(_session, _id)};
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
using ScopedTrackBookmark = ScopedEntity<Database::TrackBookmark>;
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
		Database::Db& getDb() { return _db; }

	private:
		const std::filesystem::path _tmpFile {std::tmpnam(nullptr)};
		ScopedFileDeleter fileDeleter {_tmpFile};
		Database::Db _db {_tmpFile};
};

class DatabaseFixture : public ::testing::Test
{
public:
	~DatabaseFixture()
	{
		testDatabaseEmpty();
	}

public:
    static void SetUpTestCase()
	{
		_tmpDb = std::make_unique<TmpDatabase>();
		{
			Database::Session s {_tmpDb->getDb()};
			s.prepareTables();
			s.optimize();

			// remove default created entries
			{
				auto transaction {s.createUniqueTransaction()};
				auto clusterTypes {Database::ClusterType::getAll(s)};
				for (auto& clusterType : clusterTypes)
					clusterType.remove();
			}
		}
    }

    static void TearDownTestCase()
	{
		_tmpDb.reset();
    }

private:
	void testDatabaseEmpty()
	{
		auto uniqueTransaction {session.createUniqueTransaction()};

		EXPECT_TRUE(Database::Artist::getAll(session, Database::Artist::SortMethod::ByName).empty());
		EXPECT_TRUE(Database::Cluster::getAll(session).empty());
		EXPECT_TRUE(Database::ClusterType::getAll(session).empty());
		EXPECT_TRUE(Database::Release::getAll(session).empty());
		EXPECT_TRUE(Database::Track::getAll(session).empty());
		EXPECT_TRUE(Database::TrackBookmark::getAll(session).empty());
		EXPECT_TRUE(Database::TrackList::getAll(session).empty());
		EXPECT_TRUE(Database::User::getAll(session).empty());
	}

	static inline std::unique_ptr<TmpDatabase> _tmpDb {};

public:
	Database::Session session {_tmpDb->getDb()};
};

