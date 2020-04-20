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
std::optional<Av::Format>
AudioFormatToAvFormat(Database::AudioFormat format)
{
	switch (format)
	{
		case Database::AudioFormat::MP3:		return Av::Format::MP3;
		case Database::AudioFormat::OGG_OPUS:		return Av::Format::OGG_OPUS;
		case Database::AudioFormat::MATROSKA_OPUS:	return Av::Format::MATROSKA_OPUS;
		case Database::AudioFormat::OGG_VORBIS:		return Av::Format::OGG_VORBIS;
		case Database::AudioFormat::WEBM_VORBIS:	return Av::Format::WEBM_VORBIS;
	}

	LOG(ERROR) << "Cannot convert from audio format to AV format";

	return std::nullopt;
}

std::string
AudioTranscodeResource::getUrl(Database::IdType trackId) const
{
	return url() + "&trackid=" + std::to_string(trackId);
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

void
AudioTranscodeResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	std::shared_ptr<Av::Transcoder> transcoder;

	// First, see if this request is for a continuation
	Wt::Http::ResponseContinuation *continuation = request.continuation();
	if (continuation)
	{
		LOG(DEBUG) << "Continuation! " << continuation ;
		transcoder = Wt::cpp17::any_cast<std::shared_ptr<Av::Transcoder>>(continuation->data());
	}
	else
	{
		LOG(DEBUG) << "First request: creating transcoder";

		// mandatory parameters
		auto trackId {readParameterAs<Database::IdType>(request, "trackid")};
		auto format {readParameterAs<Database::AudioFormat>(request, "format")};
		auto bitrate {readParameterAs<Database::Bitrate>(request, "bitrate")};

		if (!trackId || !format || !bitrate)
			return;

		auto avFormat {AudioFormatToAvFormat(*format)};
		if (!avFormat)
			return;

		// optional parameter
		auto offset {readParameterAs<std::size_t>(request, "offset")};

		std::filesystem::path trackPath;
		{
			// DbSession are not thread safe
			Wt::WApplication::UpdateLock lock(LmsApp);
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), *trackId)};
			if (!track)
			{
				LOG(ERROR) << "Missing track";
				return;
			}

			trackPath = track->getPath();

			if (Database::User::audioTranscodeAllowedBitrates.find(*bitrate) == std::cend(Database::User::audioTranscodeAllowedBitrates))
			{
				LOG(ERROR) << "Bitrate '" << *bitrate << "' is not allowed";
				return;
			}
		}

		Av::TranscodeParameters parameters {};
		parameters.stripMetadata = true;
		parameters.format = *avFormat;
		parameters.bitrate = *bitrate;
		parameters.offset = std::chrono::seconds {offset ? *offset : 0};

		transcoder = std::make_shared<Av::Transcoder>(trackPath, parameters);
		if (!transcoder->start())
		{
			LOG(ERROR) << "Cannot start transcoder";
			return;
		}

		LOG(DEBUG) << "Transcoder started";

		std::string mimeType {transcoder->getOutputMimeType()};
		response.setMimeType(mimeType);
		LOG(DEBUG) << "Mime type set to '" << mimeType << "'";

	}

	if (!transcoder->isComplete())
	{
		std::vector<unsigned char> data;
		data.reserve(_chunkSize);

		transcoder->process(data, _chunkSize);

		response.out().write(reinterpret_cast<char*>(&data[0]), data.size());
		LOG(DEBUG) << "Written " << data.size() << " bytes! complete = " << (transcoder->isComplete() ? "true" : "false");

		if (!response.out())
		{
			LOG(ERROR) << "Write failed!";
		}
	}

	if (!transcoder->isComplete() && response.out())
	{
		continuation = response.createContinuation();
		continuation->setData(transcoder);
	}
	else
		LOG(DEBUG) << "No more data!";
}

} // namespace UserInterface


