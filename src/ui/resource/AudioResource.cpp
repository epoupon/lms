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
#include "database/User.hpp"

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

static
std::unique_ptr<Av::Transcoder>
createTranscoder(const Wt::Http::Request& request, std::size_t bufferSize)
{
	Database::IdType trackId;
	Av::TranscodeParameters parameters {};
	parameters.stripMetadata = true;

	LMS_LOG(UI, DEBUG) << "Creating transcoder";
	try
	{
		auto trackIdStr = request.getParameter("trackid");
		if (!trackIdStr)
		{
			LMS_LOG(UI, ERROR) << "Missing trackid transcode parameter!";
			return {};
		}
		trackId = std::stol(*request.getParameter("trackid"));

		auto offsetStr = request.getParameter("offset");
		parameters.offset = std::chrono::seconds(offsetStr ? std::stol(*offsetStr) : 0);
	}
	catch (std::exception &e)
	{
		LMS_LOG(UI, ERROR) << "Exception while handling URL parameters: " << e.what();
		return {};
	}

	// DbSession are not thread safe
	{
		Wt::WApplication::UpdateLock lock {LmsApp};
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
		if (!track)
		{
			LMS_LOG(UI, ERROR) << "Missing track";
			return {};
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
				case Database::AudioFormat::MATROSKA_OPUS:
					parameters.encoding = Av::Encoding::MATROSKA_OPUS;
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

		return std::make_unique<Av::Transcoder>(track->getPath(), parameters, bufferSize);
	}
}


void
AudioResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	auto waitForMoreData = [](std::shared_ptr<Av::Transcoder> transcoder, Wt::Http::ResponseContinuation& continuation)
	{
		LMS_LOG(UI, DEBUG) << "Waiting for more data";
		transcoder->asyncWaitForData([&]()
		{
			continuation.haveMoreData();
		});
		continuation.waitForMoreData();
	};

	std::shared_ptr<Av::Transcoder> transcoder;

	Wt::Http::ResponseContinuation *continuation = request.continuation();
	if (!continuation)
	{
		transcoder = createTranscoder(request, _chunkSize);
		if (!transcoder)
		{
			LMS_LOG(UI, ERROR) << "Cannot create transcoder";
			return;
		}

		if (!transcoder->start())
		{
			LMS_LOG(UI, ERROR) << "Cannot create transcoder";
			return;
		}

		response.setMimeType(transcoder->getOutputMimeType());
		LMS_LOG(UI, DEBUG) << "Mime type set to '" << transcoder->getOutputMimeType() << "'";
	}
	else
	{
		LMS_LOG(UI, DEBUG) << "Continuation! " << continuation ;
		transcoder = Wt::cpp17::any_cast<std::shared_ptr<Av::Transcoder>>(continuation->data());
		if (!transcoder)
		{
			LMS_LOG(UI, ERROR) << "Cannot retrieve transcoder";
			return;
		}

		transcoder->consumeData([&](const unsigned char* data, std::size_t size)
		{
			response.out().write(reinterpret_cast<const char*>(data), size);
			LMS_LOG(UI, DEBUG) << "Written " << size << " bytes! finished = " << (transcoder->finished() ? "true" : "false");

			return size;
		});
	}

	if (!transcoder->finished())
	{
		continuation = response.createContinuation();
		continuation->setData(transcoder);

		waitForMoreData(transcoder, *continuation);
	}
}

} // namespace UserInterface


