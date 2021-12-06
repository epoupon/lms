
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

#include "services/database/TrackId.hpp"
#include "services/database/UserId.hpp"

namespace Database
{
	class Db;
	class Session;
}

namespace Http
{
	class IClient;
}

namespace Scrobbling::ListenBrainz
{
	struct Feedback
	{
		enum class Type
		{
			// See https://listenbrainz.readthedocs.io/en/production/dev/feedback-json/#feedback-json-doc
			Love = 1,
			Hate = -1,
			Erase = 0,
		};

		Type				type;
		Database::UserId 	userId;
		Database::TrackId 	trackId;
	};

	class FeedbackSender
	{
		public:
			FeedbackSender(Database::Db& db, Http::IClient& client);

			void enqueFeedback(const Feedback& feedback);

		private:
			std::string		feedbackToJsonString(Database::Session& session, const Feedback& feedback);
			Database::Db&	_db;
			Http::IClient&	_client;
	};
}
