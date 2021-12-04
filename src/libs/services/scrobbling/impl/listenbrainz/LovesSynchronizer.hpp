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

#include <optional>
#include <unordered_map>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/steady_timer.hpp>

#include "services/database/Types.hpp"
#include "services/scrobbling/Listen.hpp"

namespace Database
{
	class Db;
	class Session;
	class TrackList;
	class User;
}

namespace Scrobbling::ListenBrainz
{
	class ListensSynchronizer
	{
		public:
			ListensSynchronizer(boost::asio::io_context& ioContext, Database::Db& db, std::string_view baseAPIUrl);

			void saveListen(const TimedListen& listen);

		private:
			struct UserContext
			{
				UserContext(Database::UserId id) : userId {id} {}

				UserContext(const UserContext&) = delete;
				UserContext(UserContext&&) = delete;
				UserContext& operator=(const UserContext&) = delete;
				UserContext& operator=(UserContext&&) = delete;

				const Database::UserId		userId;
				bool						fetching {};

				// resetted at each fetch
				std::string		listenBrainzUserName; // need to be resolved first
				Wt::WDateTime	maxDateTime;
				std::size_t		fetchedLoveCount{};
				std::size_t		matchedLoveCount{};
				std::size_t		importedLoveCount{};
			};

			UserContext& getUserContext(Database::UserId userId);
			bool isFetching() const;
			void scheduleGetListens(std::chrono::seconds fromNow);
			void startGetListens();
			void startGetListens(UserContext& context);
			void onGetListensEnded(UserContext& context);
			void enqueValidateToken(UserContext& context);
			void enqueGetListenCount(UserContext& context);
			void enqueGetListens(UserContext& context);
			void									processGetListensResponse(std::string_view body, UserContext& context);

			boost::asio::io_context&		_ioContext;
			boost::asio::io_context::strand	_strand {_ioContext};
			Database::Db&					_db;
			std::string						_baseAPIUrl;
			boost::asio::steady_timer		_getListensTimer {_ioContext};

			std::unordered_map<Database::UserId, UserContext> _userContexts;

			const std::size_t			_maxSyncListenCount;
			const std::chrono::hours	_syncListensPeriod;
	};
} // Scrobbling::ListenBrainz

