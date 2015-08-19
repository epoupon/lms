/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "logger/Logger.hpp"

#include "RequestHandler.hpp"

namespace LmsAPI {
namespace Server {

RequestHandler::RequestHandler(Wt::Dbo::SqlConnectionPool &connectionPool)
: _db( connectionPool ),
_authRequestHandler(_db),
_audioCollectionRequestHandler(_db),
_mediaRequestHandler(_db)
{
}

RequestHandler::~RequestHandler()
{
	// TODO manually log out user if needed?
	_db.getLogin().logout();
}

bool
RequestHandler::process(const ClientMessage& request, ServerMessage& response)
{
	bool res = false;

	switch(request.type())
	{

		case ClientMessage::AuthRequest:
			if (request.has_auth_request())
			{
				res = _authRequestHandler.process(request.auth_request(), *response.mutable_auth_response());
				if (res)
					response.set_type(ServerMessage::AuthResponse);
			}
			else
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad ClientMessage::AuthRequest !";
			break;
		case ClientMessage::AudioCollectionRequest:
			// Not allowed if the user is not logged in
			if (_db.getLogin().loggedIn())
			{
				if (request.has_audio_collection_request())
				{
					res = _audioCollectionRequestHandler.process(request.audio_collection_request(), *response.mutable_audio_collection_response());
					if (res)
						response.set_type( ServerMessage::AudioCollectionResponse);
				}
				else
					LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Bad ClientMessage::AudioCollectionRequest message!";
			}
			break;

		case ClientMessage::MediaRequest:
			// Not allowed if the user is not logged in
			if (_db.getLogin().loggedIn())
			{
				if (request.has_media_request())
				{
					res = _mediaRequestHandler.process(request.media_request(), *response.mutable_media_response());
					if (res)
						response.set_type( ServerMessage::MediaResponse);
				}
				else
					LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Malformed ClientMessage::MediaRequest message!";
			}
			break;

		default:
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Unhandled message type = " << request.type();
	}

	return res;
}


} // namespace Server
} // namespace LmsAPI



