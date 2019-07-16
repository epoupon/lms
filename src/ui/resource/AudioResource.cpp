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

#include "AudioResource.hpp"

#include <Wt/Http/Response.h>

#include "utils/Logger.hpp"

#include "database/Track.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {


AudioResource:: ~AudioResource()
{
	beingDeleted();
}

std::string
AudioResource::getUrl(Database::IdType trackId) const
{
	return url()+ "&trackid=" + std::to_string(trackId);
}

void
AudioResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	std::shared_ptr<Av::Transcoder> transcoder;

	// First, see if this request is for a continuation
	Wt::Http::ResponseContinuation *continuation = request.continuation();
	if (continuation)
	{
		LMS_LOG(UI, DEBUG) << "Continuation! " << continuation ;
		transcoder = Wt::cpp17::any_cast<std::shared_ptr<Av::Transcoder>>(continuation->data());
	}
	else
	{
		Database::IdType trackId;
		Av::TranscodeParameters parameters {};
		parameters.stripMetadata = true;

		LMS_LOG(UI, DEBUG) << "First request: creating transcoder";
		try
		{
			auto trackIdStr = request.getParameter("trackid");
			if (!trackIdStr)
			{
				LMS_LOG(UI, ERROR) << "Missing trackid transcode parameter!";
				return;
			}
			trackId = std::stol(*request.getParameter("trackid"));

			auto offsetStr = request.getParameter("offset");
			parameters.offset = std::chrono::seconds(offsetStr ? std::stol(*offsetStr) : 0);
		}
		catch (std::exception &e)
		{
			LMS_LOG(UI, ERROR) << "Exception while handling URL parameters: " << e.what();
			return;
		}

		// DbSession are not thread safe
		{
			Wt::WApplication::UpdateLock lock(LmsApp);
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
			if (!track)
			{
				LMS_LOG(UI, ERROR) << "Missing track";
				return;
			}

			if (LmsApp->getUser()->getAudioTranscodeEnable())
			{
				parameters.bitrate = LmsApp->getUser()->getAudioTranscodeBitrate();

				switch (LmsApp->getUser()->getAudioTranscodeFormat())
				{
					case Database::AudioFormat::MP3:
						parameters.encoding = Av::Encoding::MP3;
						break;
					case Database::AudioFormat::OGG_OPUS:
						parameters.encoding = Av::Encoding::OGG_OPUS;
						break;
					case Database::AudioFormat::OGG_VORBIS:
						parameters.encoding = Av::Encoding::OGG_VORBIS;
						break;
					case Database::AudioFormat::WEBM_VORBIS:
						parameters.encoding = Av::Encoding::WEBM_VORBIS;
						break;
					default:
						parameters.encoding = Av::Encoding::OGG_OPUS;
						break;
				}
			}
			else
				parameters.bitrate = 0;

			transcoder = std::make_shared<Av::Transcoder>(track->getPath(), parameters);
		}


		if (!transcoder->start())
		{
			LMS_LOG(UI, ERROR) << "Cannot start transcoder";
			return;
		}

		LMS_LOG(UI, DEBUG) << "Transcoder started";

		std::string mimeType {transcoder->getOutputMimeType()};
		response.setMimeType(mimeType);
		LMS_LOG(UI, DEBUG) << "Mime type set to '" << mimeType << "'";

	}

	if (!transcoder->isComplete())
	{
		std::vector<unsigned char> data;
		data.reserve(_chunkSize);

		transcoder->process(data, _chunkSize);

		response.out().write(reinterpret_cast<char*>(&data[0]), data.size());
		LMS_LOG(UI, DEBUG) << "Written " << data.size() << " bytes! complete = " << (transcoder->isComplete() ? "true" : "false");

		if (!response.out())
		{
			LMS_LOG(UI, ERROR) << "Write failed!";
		}
	}

	if (!transcoder->isComplete() && response.out())
	{
		continuation = response.createContinuation();
		continuation->setData(transcoder);
	}
	else
		LMS_LOG(UI, DEBUG) << "No more data!";
}

} // namespace UserInterface


