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

#ifndef REMOTE_MEDIA_REQUEST_HANDLER
#define REMOTE_MEDIA_REQUEST_HANDLER

#include <map>
#include <memory>

#include "messages/media.pb.h"

#include "database/DatabaseHandler.hpp"
#include "transcode/AvConvTranscoder.hpp"

namespace Remote {
namespace Server {

class MediaRequestHandler
{
	public:
		MediaRequestHandler(Database::Handler& db);

		bool process(const MediaRequest& request, MediaResponse& response);

	private:

		bool processAudioPrepare(const MediaRequest::Prepare::Audio& request, MediaResponse::PrepareResult& response);
		bool processGetPart(const MediaRequest::GetPart& request, MediaResponse::PartResult& response);
		bool processTerminate(const MediaRequest::Terminate& request, MediaResponse::TerminateResult& response);

//		bool processVideoPrepare(const AudioCollectionRequest::GetGenreList& request, AudioCollectionResponse::GenreList& response);

		std::map<uint32_t, std::shared_ptr<Transcode::AvConvTranscoder> > _transcoders;

		Database::Handler& _db;

		uint32_t _curHandle = 0;

		static const std::size_t _maxPartSize = 65536 - 128;
		static const std::size_t _maxTranscoders = 1;
};

} // namespace Remote
} // namespace Server

#endif

