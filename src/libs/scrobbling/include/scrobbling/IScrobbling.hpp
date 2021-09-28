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

#include "scrobbling/Listen.hpp"
#include "database/Types.hpp"

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

	class IScrobbling
	{
		public:
			virtual ~IScrobbling() = default;

			// Scrobbling
			virtual void listenStarted(const Listen& listen) = 0;
			virtual void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> playedDuration = std::nullopt) = 0;

			virtual void addTimedListen(const TimedListen& listen) = 0;

			// Stats
			// From most recent to oldest
			virtual std::vector<Database::ObjectPtr<Database::Artist>> getRecentArtists(Database::Session& session,
																	Database::ObjectPtr<Database::User> user,
																	const std::vector<Database::ClusterId>& clusterIds,
																	std::optional<Database::TrackArtistLinkType> linkType,
																	std::optional<Database::Range> range,
																	bool& moreResults) = 0;

			virtual std::vector<Database::ObjectPtr<Database::Release>> getRecentReleases(Database::Session& session,
																	Database::ObjectPtr<Database::User> user,
																	const std::vector<Database::ClusterId>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) = 0;

			virtual std::vector<Database::ObjectPtr<Database::Track>> getRecentTracks(Database::Session& session,
																	Database::ObjectPtr<Database::User> user,
																	const std::vector<Database::ClusterId>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) = 0;

			// Top
			virtual std::vector<Database::ObjectPtr<Database::Artist>> getTopArtists(Database::Session& session,
																	Database::ObjectPtr<Database::User> user,
																	const std::vector<Database::ClusterId>& clusterIds,
																	std::optional<Database::TrackArtistLinkType> linkType,
																	std::optional<Database::Range> range,
																	bool& moreResults) = 0;

			virtual std::vector<Database::ObjectPtr<Database::Release>> getTopReleases(Database::Session& session,
																	Database::ObjectPtr<Database::User> user,
																	const std::vector<Database::ClusterId>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) = 0;

			virtual std::vector<Database::ObjectPtr<Database::Track>> getTopTracks(Database::Session& session,
																	Database::ObjectPtr<Database::User> user,
																	const std::vector<Database::ClusterId>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) = 0;
	};

	std::unique_ptr<IScrobbling> createScrobbling(boost::asio::io_service& ioService, Database::Db& db);

} // ns Scrobbling

