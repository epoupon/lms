
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
#include "services/database/Track.hpp"
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
	FeedbackSender::enqueFeedback(const Feedback& feedback)
	{
		Http::ClientPOSTRequestParameters request;
		request.relativeUrl = "/1/feedback/recording-feedback";

		std::string bodyText {feedbackToJsonString(_db.getTLSSession(), feedback)};
		if (bodyText.empty())
		{
			LOG(DEBUG) << "Cannot convert feedback to json: skipping";
			return;
		}

		const std::optional<UUID> listenBrainzToken {Utils::getListenBrainzToken(_db.getTLSSession(), feedback.userId)};
		if (!listenBrainzToken)
			return;

		request.message.addBodyText(bodyText);
		request.message.addHeader("Authorization", "Token " + std::string {listenBrainzToken->getAsString()});
		request.message.addHeader("Content-Type", "application/json");
		_client.sendPOSTRequest(std::move(request));
	}

	std::string
	FeedbackSender::feedbackToJsonString(Session& session, const Feedback& feedback)
	{
		std::string res;

		std::string recordingMBID;
		{
			auto transaction {session.createSharedTransaction()};

			const Track::pointer track {Track::find(session, feedback.trackId)};
			if (!track)
				return res;

			if (auto mbid {track->getRecordingMBID()})
				recordingMBID = mbid->getAsString();
		}

		if (recordingMBID.empty())
		{
			LOG(DEBUG) << "Track has no recording MBID: skipping";
			return res;
		}

		Wt::Json::Object root;
		root["recording_msid"] = Wt::Json::Value {recordingMBID};
		root["score"] = Wt::Json::Value {static_cast<int>(feedback.type)};

		res = Wt::Json::serialize(root);
		return res;
	}
}
