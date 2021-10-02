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

#include <memory>
#include <optional>
#include <unordered_map>

#include "scrobbling/IScrobbling.hpp"
#include "IScrobbler.hpp"

namespace Scrobbling
{
	class Scrobbling : public IScrobbling
	{
		public:
			Scrobbling(boost::asio::io_context& ioContext, Database::Db& db);

		private:
			void listenStarted(const Listen& listen) override;
			void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration) override;
			void addTimedListen(const TimedListen& listen) override;

			ArtistContainer getRecentArtists(Database::UserId userId,
												const std::vector<Database::ClusterId>& clusterIds,
												std::optional<Database::TrackArtistLinkType> linkType,
												std::optional<Database::Range> range,
												bool& moreResults) override;

			ReleaseContainer getRecentReleases(Database::UserId userId,
												const std::vector<Database::ClusterId>& clusterIds,
												std::optional<Database::Range> range,
												bool& moreResults) override;

			TrackContainer getRecentTracks(Database::UserId userId,
												const std::vector<Database::ClusterId>& clusterIds,
												std::optional<Database::Range> range,
												bool& moreResults) override;

			ArtistContainer getTopArtists(Database::UserId userId,
												const std::vector<Database::ClusterId>& clusterIds,
												std::optional<Database::TrackArtistLinkType> linkType,
												std::optional<Database::Range> range,
												bool& moreResults) override;

			ReleaseContainer getTopReleases(Database::UserId userId,
												const std::vector<Database::ClusterId>& clusterIds,
												std::optional<Database::Range> range,
												bool& moreResults) override;

			TrackContainer getTopTracks(Database::UserId userId,
												const std::vector<Database::ClusterId>& clusterIds,
												std::optional<Database::Range> range,
												bool& moreResults) override;

			Database::ObjectPtr<Database::TrackList> getListensTrackList(Database::Session& session, Database::ObjectPtr<Database::User> user);

			std::optional<Database::Scrobbler> getUserScrobbler(Database::UserId userId);

			Database::Db& _db;
			std::unordered_map<Database::Scrobbler, std::unique_ptr<IScrobbler>> _scrobblers;
	};

} // ns Scrobbling

