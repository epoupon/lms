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

#include <boost/asio/io_service.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <Wt/WDateTime.h>

#include "services/scrobbling/Listen.hpp"
#include "services/database/Types.hpp"

namespace Database
{
	class Artist;
	class Db;
	class Release;
	class Session;
	class Track;
	class User;
}

namespace Scrobbling
{

	class IScrobblingService
	{
		public:
			virtual ~IScrobblingService() = default;

			// Scrobbling
			virtual void listenStarted(const Listen& listen) = 0;
			virtual void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> playedDuration = std::nullopt) = 0;

			virtual void addTimedListen(const TimedListen& listen) = 0;

			// Stats
			template <typename IdType>
			using ResultContainer = std::vector<IdType>;

			using ArtistContainer = ResultContainer<Database::ArtistId>;
			using ReleaseContainer = ResultContainer<Database::ReleaseId>;
			using TrackContainer = ResultContainer<Database::TrackId>;
			// From most recent to oldest
			virtual ArtistContainer getRecentArtists(Database::UserId userId,
														const std::vector<Database::ClusterId>& clusterIds,
														std::optional<Database::TrackArtistLinkType> linkType,
														std::optional<Database::Range> range,
														bool& moreResults) = 0;

			virtual ReleaseContainer getRecentReleases(Database::UserId userId,
														const std::vector<Database::ClusterId>& clusterIds,
														std::optional<Database::Range> range,
														bool& moreResults) = 0;

			virtual TrackContainer getRecentTracks(Database::UserId userId,
														const std::vector<Database::ClusterId>& clusterIds,
														std::optional<Database::Range> range,
														bool& moreResults) = 0;

			// Top
			virtual ArtistContainer getTopArtists(Database::UserId userId,
														const std::vector<Database::ClusterId>& clusterIds,
														std::optional<Database::TrackArtistLinkType> linkType,
														std::optional<Database::Range> range,
														bool& moreResults) = 0;

			virtual ReleaseContainer getTopReleases(Database::UserId userId,
														const std::vector<Database::ClusterId>& clusterIds,
														std::optional<Database::Range> range,
														bool& moreResults) = 0;

			virtual TrackContainer getTopTracks(Database::UserId userId,
														const std::vector<Database::ClusterId>& clusterIds,
														std::optional<Database::Range> range,
														bool& moreResults) = 0;
	};

	std::unique_ptr<IScrobblingService> createScrobblingService(boost::asio::io_service& ioService, Database::Db& db);

} // ns Scrobbling

