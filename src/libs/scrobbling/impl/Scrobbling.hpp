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

			Scrobbling(Database::Db& db);

		private:
			void listenStarted(const Listen& listen) override;
			void listenFinished(const Listen& listen, std::chrono::seconds duration) override;
			void addListen(const Listen& listen, Wt::WDateTime timePoint) override;

			std::vector<Wt::Dbo::ptr<Database::Artist>> getRecentArtists(Database::Session& session,
																	Wt::Dbo::ptr<Database::User> user,
																	const std::set<Database::IdType>& clusterIds,
																	std::optional<Database::TrackArtistLinkType> linkType,
																	std::optional<Database::Range> range,
																	bool& moreResults) override;

			std::vector<Wt::Dbo::ptr<Database::Release>> getRecentReleases(Database::Session& session,
																	Wt::Dbo::ptr<Database::User> user,
																	const std::set<Database::IdType>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) override;

			std::vector<Wt::Dbo::ptr<Database::Track>> getRecentTracks(Database::Session& session,
																	Wt::Dbo::ptr<Database::User> user,
																	const std::set<Database::IdType>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) override;

			std::vector<Wt::Dbo::ptr<Database::Artist>> getTopArtists(Database::Session& session,
																	Wt::Dbo::ptr<Database::User> user,
																	const std::set<Database::IdType>& clusterIds,
																	std::optional<Database::TrackArtistLinkType> linkType,
																	std::optional<Database::Range> range,
																	bool& moreResults) override;

			std::vector<Wt::Dbo::ptr<Database::Release>> getTopReleases(Database::Session& session,
																	Wt::Dbo::ptr<Database::User> user,
																	const std::set<Database::IdType>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) override;

			std::vector<Wt::Dbo::ptr<Database::Track>> getTopTracks(Database::Session& session,
																	Wt::Dbo::ptr<Database::User> user,
																	const std::set<Database::IdType>& clusterIds,
																	std::optional<Database::Range> range,
																	bool& moreResults) override;

			Wt::Dbo::ptr<Database::TrackList> getListensTrackList(Database::Session& session, Wt::Dbo::ptr<Database::User> user);

			std::optional<Database::Scrobbler> getUserScrobbler(Database::IdType userId);

			Database::Db& _db;
			std::unordered_map<Database::Scrobbler, std::unique_ptr<IScrobbler>> _scrobblers;
	};

} // ns Scrobbling

