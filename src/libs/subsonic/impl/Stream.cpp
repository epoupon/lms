/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "Stream.hpp"

#include "av/AvTranscoder.hpp"
#include "av/AvTranscodeResourceHandlerCreator.hpp"
#include "av/AvTypes.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"
#include "utils/IResourceHandler.hpp"
#include "utils/FileResourceHandlerCreator.hpp"
#include "utils/Utils.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

using namespace Database;

namespace API::Subsonic::Stream
{

static
Av::Encoding
userTranscodeFormatToAvEncoding(AudioFormat format)
{
	switch (format)
	{
		case AudioFormat::MP3:			return Av::Encoding::MP3;
		case AudioFormat::OGG_OPUS:		return Av::Encoding::OGG_OPUS;
		case AudioFormat::MATROSKA_OPUS:	return Av::Encoding::MATROSKA_OPUS;
		case AudioFormat::OGG_VORBIS:		return Av::Encoding::OGG_VORBIS;
		case AudioFormat::WEBM_VORBIS:		return Av::Encoding::WEBM_VORBIS;
		default:				return Av::Encoding::OGG_OPUS;
	}
}

struct StreamParameters
{
	std::filesystem::path trackPath;
	std::optional<Av::TranscodeParameters> transcodeParameters;
};


static
StreamParameters
getStreamParameters(RequestContext& context)
{
	// Mandatory params
	Id id {getMandatoryParameterAs<Id>(context.parameters, "id")};

	// Optional params
	std::optional<std::size_t> maxBitRate {getParameterAs<std::size_t>(context.parameters, "maxBitRate")};
	std::optional<std::string> format {getParameterAs<std::string>(context.parameters, "format")};

	StreamParameters parameters;

	auto transaction {context.dbSession.createSharedTransaction()};

	{
		auto track {Track::getById(context.dbSession, id.value)};
		if (!track)
			throw RequestedDataNotFoundError {};

		parameters.trackPath = track->getPath();
	}

	{
		const User::pointer user {User::getByLoginName(context.dbSession, context.userName)};
		if (!user)
			throw UserNotAuthorizedError {};

		// format = "raw" => no transcode. Other format values will be ignored
		const bool transcode {(!format || (*format != "raw")) && user->getAudioTranscodeEnable()};
		if (transcode)
		{
			// "If set to zero, no limit is imposed"
			if (!maxBitRate || *maxBitRate == 0)
				maxBitRate = user->getAudioTranscodeBitrate() / 1000;

			*maxBitRate = clamp(*maxBitRate, std::size_t {48}, user->getMaxAudioTranscodeBitrate() / 1000);

			Av::TranscodeParameters transcodeParameters;

			transcodeParameters.bitrate = *maxBitRate * 1000;
			transcodeParameters.encoding = userTranscodeFormatToAvEncoding(user->getAudioTranscodeFormat());

			parameters.transcodeParameters = std::move(transcodeParameters);
		}
	}

	return parameters;
}

void
handle(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
{
	std::shared_ptr<IResourceHandler> resourceHandler;

	Wt::Http::ResponseContinuation *continuation = request.continuation();
	if (!continuation)
	{
		StreamParameters streamParameters {getStreamParameters(context)};
		if (streamParameters.transcodeParameters)
			resourceHandler = Av::createTranscodeResourceHandler(streamParameters.trackPath, *streamParameters.transcodeParameters);
		else
			resourceHandler = createFileResourceHandler(streamParameters.trackPath);
	}
	else
	{
		resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
	}

	resourceHandler->processRequest(request, response);
	if (!resourceHandler->isFinished())
	{
		Wt::Http::ResponseContinuation *continuation = response.createContinuation();
		continuation->setData(resourceHandler);
	}
}

}

