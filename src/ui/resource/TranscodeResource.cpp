/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <Wt/Http/Response>

#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "TranscodeResource.hpp"

namespace UserInterface {

TranscodeResource::TranscodeResource(Database::Handler& db, Wt::WObject *parent)
:  Wt::WResource(parent),
_db(db)
{
	LMS_LOG(UI, DEBUG) << "CONSTRUCTING RESOURCE";
}

TranscodeResource:: ~TranscodeResource()
{
	LMS_LOG(UI, DEBUG) << "DESTRUCTING RESOURCE";
	beingDeleted();
}

std::string
TranscodeResource::getUrl(Database::Track::id_type trackId, Av::Encoding encoding, std::size_t offset, std::vector<std::size_t> streamIds) const
{
	std::string res = url()+ "&trackid=" + std::to_string(trackId) + "&encoding=" + std::to_string(Av::encoding_to_int(encoding));
	if (!streamIds.empty())
	{
		for (std::size_t streamId : streamIds)
			res += "&stream=" + std::to_string(streamId);
	}
	res += "&offset=" + std::to_string(offset);
	return res;
}

void
TranscodeResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	// Retrieve parameters
	const std::string *trackIdStr = request.getParameter("trackid");
	const std::string *offsetStr = request.getParameter("offset");
	const std::string *encodingStr = request.getParameter("encoding");
	std::vector<std::string> streams = request.getParameterValues ("stream");

	LMS_LOG(UI, DEBUG) << "Handling new request...";

	try
	{
		std::shared_ptr<Av::Transcoder> transcoder;

		// First, see if this request is for a continuation
		Wt::Http::ResponseContinuation *continuation = request.continuation();
		if (continuation)
		{
			LMS_LOG(UI, DEBUG) << "Continuation! " << continuation ;
			transcoder = boost::any_cast<std::shared_ptr<Av::Transcoder> >(continuation->data());
			if (!transcoder)
			{
				LMS_LOG(UI, ERROR) << "No transcoder set -> abort!";
				return;
			}
		}
		else
		{
			LMS_LOG(UI, DEBUG) << "No continuation yet";

			if (!trackIdStr
				|| !offsetStr
				|| !encodingStr)
			{
				LMS_LOG(UI, ERROR) << "Missing transcode parameter";
				return;
			}

			Database::Track::id_type trackId = std::stol(*trackIdStr);

			// transactions are not thread safe
			{
				Wt::WApplication::UpdateLock lock(LmsApplication::instance());

				Wt::Dbo::Transaction transaction(_db.getSession());

				Database::User::pointer user = _db.getCurrentUser();
				Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);

				if (!track)
				{
					LMS_LOG(UI, ERROR) << "Missing track";
					return;
				}

				if (!user)
				{
					LMS_LOG(UI, ERROR) << "Missing user";
					return;
				}

				Av::TranscodeParameters parameters;

				parameters.setOffset(boost::posix_time::seconds(std::stol(*offsetStr)));
				parameters.setEncoding(Av::encoding_from_int(std::stol(*encodingStr)));
				parameters.setBitrate(Av::Stream::Type::Audio, user->getAudioBitrate() );
				for (std::string strStream: streams)
				{
					LMS_LOG(UI, DEBUG) << "Added stream " << std::stol(strStream);
					parameters.addStream(std::stol(strStream));
				}

				LMS_LOG(UI, DEBUG) << "Offset set to " << parameters.getOffset();
				transcoder = std::make_shared<Av::Transcoder>(track->getPath(), parameters);
			}

			std::string mimeType = Av::encoding_to_mimetype(transcoder->getParameters().getEncoding());

			LMS_LOG(UI, DEBUG) << "Mime type set to '" << mimeType << "'";
			response.setMimeType(mimeType);

			if (!transcoder->start())
			{
				LMS_LOG(UI, ERROR) << "Cannot start transcoder";
				return;
			}

			LMS_LOG(UI, DEBUG) << "Transcoder started";
		}

		if (!transcoder->isComplete())
		{
			std::vector<unsigned char> data;
			data.reserve(_chunkSize);

			transcoder->process(data, _chunkSize);

			response.out().write(reinterpret_cast<char*>(&data[0]), data.size());
			LMS_LOG(UI, DEBUG) << "Written " << data.size() << " bytes! complete = " << std::boolalpha << transcoder->isComplete();

			if (!response.out())
			{
				LMS_LOG(UI, ERROR) << "Write failed!";
			}
		}

		if (!transcoder->isComplete() && response.out()) {
			continuation = response.createContinuation();
			continuation->setData(transcoder);
		}
		else
			LMS_LOG(UI, DEBUG) << "No more data!";
	}
	catch (std::invalid_argument& e)
	{
		LMS_LOG(UI, ERROR) << "Invalid argument: " << e.what();
	}
}

} // namespace UserInterface


