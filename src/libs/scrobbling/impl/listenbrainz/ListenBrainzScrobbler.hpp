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

#include <chrono>
#include <deque>
#include <mutex>

#include <Wt/Http/Client.h>
#include <Wt/WIOService.h>

#include "IScrobbler.hpp"

namespace Database
{
	class Db;
	class Session;
	class TrackList;
}

namespace Scrobbling
{
	class ListenBrainzScrobbler final : public IScrobbler
	{
		public:
			ListenBrainzScrobbler(Database::Db& db);
			~ListenBrainzScrobbler();

			ListenBrainzScrobbler(const ListenBrainzScrobbler&) = delete;
			ListenBrainzScrobbler(const ListenBrainzScrobbler&&) = delete;
			ListenBrainzScrobbler& operator=(const ListenBrainzScrobbler&) = delete;
			ListenBrainzScrobbler& operator=(const ListenBrainzScrobbler&&) = delete;

		private:
			void listenStarted(const Listen& listen) override;
			void listenFinished(const Listen& listen, std::chrono::seconds duration) override;
			void addListen(const Listen& listen, const Wt::WDateTime& timePoint) override;

			Wt::Dbo::ptr<Database::TrackList> getListensTrackList(Database::Session& session, Wt::Dbo::ptr<Database::User> user) override;

			void enqueListen(const Listen& listen, const Wt::WDateTime& timePoint);
			void sendNextQueuedListen();
			bool sendListen(const Listen& listen, const Wt::WDateTime& timePoint);
			void onClientDone(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& msg);
			void throttle(std::chrono::seconds duration);

			void cacheListen(const Listen& listen, const Wt::WDateTime& timePoint);

			enum class State
			{
				Idle,
				Throttled,
				Sending,
			};
			State						_state {State::Idle};

			const std::string			_apiEndpoint;
			const std::size_t			_maxRetryCount {2};
			const std::chrono::seconds	_defaultRetryWaitDuration {30};
			const std::chrono::seconds	_minRetryWaitDuration {1};
			const std::chrono::seconds	_maxRetryWaitDuration {300};

			Database::Db&				_db;
			Wt::WIOService				_ioService;
			Wt::Http::Client			_client {_ioService};

			struct QueuedListen
			{
				Listen			listen;
				Wt::WDateTime	timePoint;
				std::size_t		retryCount {};
			};
			std::deque<QueuedListen>	_sendQueue;
	};
} // Scrobbling

