
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

#include "FeedbackSender.hpp"

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/StarredTrack.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "services/scrobbling/Exception.hpp"
#include "utils/http/IClient.hpp"

#include "Utils.hpp"

using namespace Database;

namespace Scrobbling::ListenBrainz
{
	FeedbackSender::FeedbackSender(Database::Db& db, Http::IClient& client)
		: _db {db}
	, _client {client}
	{}

	void
	FeedbackSender::enqueFeedback(FeedbackType type, Database::StarredTrackId starredTrackId)
	{
		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		StarredTrack::pointer starredTrack {StarredTrack::find(session, starredTrackId)};
		if (!starredTrack)
			return;

		Http::ClientPOSTRequestParameters request;
		request.relativeUrl = "/1/feedback/recording-feedback";

		std::optional<UUID> recordingMBID {starredTrack->getTrack()->getRecordingMBID()};

		switch (type)
		{
			case FeedbackType::Love:
				// if there is no recording MBID, do not set to synchronized to make the user a chance to update its tags
				// and make it synchronizable later
				starredTrack.modify()->setScrobblingState(ScrobblingState::PendingAdd);
				if (!recordingMBID)
				{
					LOG(DEBUG) << "Track has no recording MBID: skipping";
					return;
				}
				break;

			case FeedbackType::Erase:
				if (!recordingMBID)
				{
					LOG(DEBUG) << "Track has no recording MBID: erasing star";
					starredTrack.remove();
					return;
				}
				else
				{
					starredTrack.modify()->setScrobblingState(ScrobblingState::PendingRemove);
				}
				break;

			default:
				throw Exception {"Unhandled feedback type"};
		}

		const std::optional<UUID> listenBrainzToken {starredTrack->getUser()->getListenBrainzToken()};
		if (!listenBrainzToken)
			return;

		request.message.addHeader("Authorization", "Token " + std::string {listenBrainzToken->getAsString()});

		Wt::Json::Object root;
		root["recording_mbid"] = Wt::Json::Value {std::string {recordingMBID->getAsString()}};
		root["score"] = Wt::Json::Value {static_cast<int>(type)};

		request.message.addBodyText(Wt::Json::serialize(root));
		request.message.addHeader("Content-Type", "application/json");

		request.onSuccessFunc = [=](std::string_view /*msgBody*/) { onFeedbackSent(type, starredTrackId); };
		_client.sendPOSTRequest(std::move(request));
	}

	void
	FeedbackSender::onFeedbackSent(FeedbackType type, Database::StarredTrackId starredTrackId)
	{
		Session& session {_db.getTLSSession()};
		auto transaction {session.createUniqueTransaction()};

		StarredTrack::pointer starredTrack {StarredTrack::find(session, starredTrackId)};
		if (!starredTrack)
		{
			LOG(DEBUG) << "Starred track not found. deleted?";
			return;
		}

		switch (type)
		{
			case FeedbackType::Love:
				starredTrack.modify()->setScrobblingState(ScrobblingState::Synchronized);
				LOG(DEBUG) << "State set to synchronized";
				break;
			case FeedbackType::Erase:
				starredTrack.remove();
				LOG(DEBUG) << "Removed starred track";
				break;

			default:
				throw Exception {"Unhandled feedback type"};
		}
	}
}
