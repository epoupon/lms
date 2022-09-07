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

#include <optional>
#include <Wt/Http/Response.h>

#include "av/TranscodeParameters.hpp"
#include "av/TranscodeResourceHandlerCreator.hpp"
#include "av/Types.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"

#include "LmsApplication.hpp"

#define LOG(level)	LMS_LOG(UI, level) << "Audio transcode resource: "

namespace StringUtils
{
	template <>
	std::optional<Database::AudioFormat>
	readAs(std::string_view str)
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

static
std::optional<Av::Format>
AudioFormatToAvFormat(Database::AudioFormat format)
{
	switch (format)
	{
		case Database::AudioFormat::MP3:			return Av::Format::MP3;
		case Database::AudioFormat::OGG_OPUS:		return Av::Format::OGG_OPUS;
		case Database::AudioFormat::MATROSKA_OPUS:	return Av::Format::MATROSKA_OPUS;
		case Database::AudioFormat::OGG_VORBIS:		return Av::Format::OGG_VORBIS;
		case Database::AudioFormat::WEBM_VORBIS:	return Av::Format::WEBM_VORBIS;
	}

	LOG(ERROR) << "Cannot convert from audio format to AV format";

	return std::nullopt;
}


namespace UserInterface {

AudioTranscodeResource:: ~AudioTranscodeResource()
{
	beingDeleted();
}

std::string
AudioTranscodeResource::getUrl(Database::TrackId trackId) const
{
	return url() + "&trackid=" + trackId.toString();
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

namespace
{
	struct TranscodeParameters
	{
		Av::InputFileParameters inputFileParameters;
		Av::TranscodeParameters transcodeParameters;
	};

	std::optional<TranscodeParameters>
	readTranscodeParameters(const Wt::Http::Request& request)
	{
		TranscodeParameters parameters;

		// mandatory parameters
		const std::optional<Database::TrackId> trackId {readParameterAs<Database::TrackId::ValueType>(request, "trackid")};
		const auto format {readParameterAs<Database::AudioFormat>(request, "format")};
		const auto bitrate {readParameterAs<Database::Bitrate>(request, "bitrate")};

		if (!trackId || !format || !bitrate)
			return std::nullopt;

		if (!Database::isAudioBitrateAllowed(*bitrate))
		{
			LOG(ERROR) << "Bitrate '" << *bitrate << "' is not allowed";
			return std::nullopt;
		}

		const std::optional<Av::Format> avFormat {AudioFormatToAvFormat(*format)};
		if (!avFormat)
			return std::nullopt;

		// optional parameter
		std::size_t offset {readParameterAs<std::size_t>(request, "offset").value_or(0)};

		std::filesystem::path trackPath;
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Track::pointer track {Database::Track::find(LmsApp->getDbSession(), *trackId)};
			if (!track)
			{
				LOG(ERROR) << "Missing track";
				return std::nullopt;
			}

			parameters.inputFileParameters.trackPath = track->getPath();
			parameters.inputFileParameters.duration = track->getDuration();
		}

		parameters.transcodeParameters.stripMetadata = true;
		parameters.transcodeParameters.format = *avFormat;
		parameters.transcodeParameters.bitrate = *bitrate;
		parameters.transcodeParameters.offset = std::chrono::seconds {offset};

		return parameters;
	}
}

void
AudioTranscodeResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	std::shared_ptr<IResourceHandler> resourceHandler;

	try
	{
		Wt::Http::ResponseContinuation* continuation {request.continuation()};
		if (!continuation)
		{
			const std::optional<TranscodeParameters>& parameters {readTranscodeParameters(request)};
			if (parameters)
				resourceHandler = Av::createTranscodeResourceHandler(parameters->inputFileParameters, parameters->transcodeParameters, false /* estimate content length */);
		}
		else
		{
			resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
		}

		if (resourceHandler)
		{
			continuation = resourceHandler->processRequest(request, response);
			if (continuation)
				continuation->setData(resourceHandler);
		}
	}
	catch (const Av::Exception& e)
	{
		LOG(ERROR) << "Caught Av exception: " << e.what();
	}
}

} // namespace UserInterface


