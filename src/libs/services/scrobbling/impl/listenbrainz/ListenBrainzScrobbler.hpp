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

#include <string>
#include <optional>
#include <boost/asio/io_context.hpp>

#include "IScrobbler.hpp"
#include "FeedbacksSynchronizer.hpp"
#include "ListensSynchronizer.hpp"

namespace Database
{
	class Db;
}

namespace Scrobbling::ListenBrainz
{
	class Scrobbler final : public IScrobbler
	{
		public:
			Scrobbler(boost::asio::io_context& ioContext, Database::Db& db);
			~Scrobbler();

			Scrobbler(const Scrobbler&) = delete;
			Scrobbler(const Scrobbler&&) = delete;
			Scrobbler& operator=(const Scrobbler&) = delete;
			Scrobbler& operator=(const Scrobbler&&) = delete;

		private:
			void listenStarted(const Listen& listen) override;
			void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration) override;
			void addTimedListen(const TimedListen& listen) override;

			// Submit listens
			void enqueListen(const Listen& listen, const Wt::WDateTime& timePoint);

			// Star
			void onStarred(Database::StarredArtistId starredArtistId) override;
			void onUnstarred(Database::StarredArtistId starredArtistId) override;
			void onStarred(Database::StarredReleaseId starredReleaseId) override;
			void onUnstarred(Database::StarredReleaseId starredReleaseId) override;
			void onStarred(Database::StarredTrackId starredTrackId) override;
			void onUnstarred(Database::StarredTrackId starredTrackId) override;

			boost::asio::io_context&		_ioContext;
			Database::Db&					_db;
			std::string						_baseAPIUrl;
			std::unique_ptr<Http::IClient>	_client;
			ListensSynchronizer				_listensSynchronizer;
			FeedbacksSynchronizer			_feedbacksSynchronizer;
	};
} // Scrobbling::ListenBrainz

