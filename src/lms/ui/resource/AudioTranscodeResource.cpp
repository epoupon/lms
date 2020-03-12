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

#include "AudioTranscodeResource.hpp"

#include <Wt/Http/Response.h>

#include "av/AvTranscoder.hpp"
#include "av/AvTranscodeResourceHandlerCreator.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "LmsApplication.hpp"

#define LOG(level)	LMS_LOG(UI, level) << "Audio transcode resource: "

namespace StringUtils
{
	template <>
	std::optional<Database::AudioFormat>
	readAs(const std::string& str)
	{

		auto encodedFormat {readAs<int>(str)};
		if (!encodedFormat)
			return std::nullopt;

		Database::AudioFormat format {static_cast<Database::AudioFormat>(*encodedFormat)};

		// check
		switch (static_cast<Database::AudioFormat>(*encodedFormat))
		{
			case Database::AudioFormat::MP3:
				[[fallthrough]];
			case Database::AudioFormat::OGG_OPUS:
				[[fallthrough]];
			case Database::AudioFormat::MATROSKA_OPUS:
				[[fallthrough]];
			case Database::AudioFormat::OGG_VORBIS:
				[[fallthrough]];
			case Database::AudioFormat::WEBM_VORBIS:
				return format;
		}

		LOG(ERROR) << "Cannot determine audio format from value '" << str << "'";

		return std::nullopt;
	}
}

namespace UserInterface {

AudioTranscodeResource:: ~AudioTranscodeResource()
{
	beingDeleted();
}

static
std::optional<Av::Encoding>
AudioFormatToAvEncoding(Database::AudioFormat format)
{
	switch (format)
	{
		case Database::AudioFormat::MP3:		return Av::Encoding::MP3;
		case Database::AudioFormat::OGG_OPUS:		return Av::Encoding::OGG_OPUS;
		case Database::AudioFormat::MATROSKA_OPUS:	return Av::Encoding::MATROSKA_OPUS;
		case Database::AudioFormat::OGG_VORBIS:		return Av::Encoding::OGG_VORBIS;
		case Database::AudioFormat::WEBM_VORBIS:	return Av::Encoding::WEBM_VORBIS;
	}

	LOG(ERROR) << "Cannot convert from audio format to encoding";

	return std::nullopt;
}

std::string
AudioTranscodeResource::getUrlForUser(Database::IdType trackId, Database::User::pointer user) const
{
	std::string computedUrl {url() + "&trackid=" + std::to_string(trackId)};

	computedUrl += "&format=" + std::to_string(static_cast<std::size_t>(user->getAudioTranscodeFormat()));
	computedUrl += "&bitrate=" + std::to_string(LmsApp->getUser()->getAudioTranscodeBitrate());

	return computedUrl;
}

template<typename T>
std::optional<T>
readParameterAs(const Wt::Http::Request& request, const std::string& parameterName)
{
	auto paramStr {request.getParameter(parameterName)};
	if (!paramStr)
	{
		LOG(DEBUG) << "Missing parameter '" << parameterName << "'";
		return std::nullopt;
	}

	auto res {StringUtils::readAs<T>(*paramStr)};
	if (!res)
		LOG(ERROR) << "Cannot parse parameter '" << parameterName << "' from value '" << *paramStr << "'";

	return res;
}

struct TranscodeParameters
{
	std::filesystem::path trackPath;
	Av::TranscodeParameters parameters;	
};

static
std::optional<TranscodeParameters>
getTranscodeParameters(const Wt::Http::Request& request)
{
	// mandatory parameters
	auto trackId {readParameterAs<Database::IdType>(request, "trackid")};
	auto format {readParameterAs<Database::AudioFormat>(request, "format")};
	auto bitrate {readParameterAs<Database::Bitrate>(request, "bitrate")};

	if (!trackId || !format || !bitrate)
		return {};

	auto encoding {AudioFormatToAvEncoding(*format)};
	if (!encoding)
		return {};

	// optional parameter
	auto offset {readParameterAs<std::size_t>(request, "offset")};

	TranscodeParameters transcodeParameters;

	{
		// DbSession are not thread safe
		Wt::WApplication::UpdateLock lock(LmsApp);
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), *trackId)};
		if (!track)
		{
			LOG(ERROR) << "Missing track";
			return {};
		}

		transcodeParameters.trackPath = track->getPath();

		if (!LmsApp->getUser()->checkBitrate(*bitrate))
		{
			LOG(ERROR) << "Bitrate '" << *bitrate << "' is not allowed";
			return {};
		}
	}

	transcodeParameters.parameters.stripMetadata = true;
	transcodeParameters.parameters.encoding = *encoding;
	transcodeParameters.parameters.bitrate = *bitrate;
	transcodeParameters.parameters.offset = std::chrono::seconds {offset ? *offset : 0};

	return transcodeParameters;
}

void
AudioTranscodeResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	std::shared_ptr<IResourceHandler> transcodeHandler;

	Wt::Http::ResponseContinuation *continuation = request.continuation();
	if (continuation)
	{
		LOG(DEBUG) << "Continuation! " << continuation ;
		transcodeHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
	}
	else
	{
		const std::optional<TranscodeParameters> transcodeParameters {getTranscodeParameters(request)};
		if (!transcodeParameters)
			return; // TODO set nice status error

		transcodeHandler = Av::createTranscodeResourceHandler(transcodeParameters->trackPath, transcodeParameters->parameters);
	}

	continuation = transcodeHandler->processRequest(request, response);
	if (continuation)
		continuation->setData(transcodeHandler);
}

} // namespace UserInterface


