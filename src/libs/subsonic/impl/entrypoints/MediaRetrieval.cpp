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

#include "MediaRetrieval.hpp"

#include "av/TranscodeParameters.hpp"
#include "av/TranscodeResourceHandlerCreator.hpp"
#include "av/Types.hpp"
#include "services/cover/ICoverService.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/IResourceHandler.hpp"
#include "utils/Logger.hpp"
#include "utils/FileResourceHandlerCreator.hpp"
#include "utils/Utils.hpp"
#include "ParameterParsing.hpp"
#include "SubsonicId.hpp"

using namespace Database;

namespace API::Subsonic
{
	namespace {
		Av::Format userTranscodeFormatToAvFormat(AudioFormat format)
		{
			switch (format)
			{
			case AudioFormat::MP3:			return Av::Format::MP3;
			case AudioFormat::OGG_OPUS:		return Av::Format::OGG_OPUS;
			case AudioFormat::MATROSKA_OPUS:	return Av::Format::MATROSKA_OPUS;
			case AudioFormat::OGG_VORBIS:		return Av::Format::OGG_VORBIS;
			case AudioFormat::WEBM_VORBIS:		return Av::Format::WEBM_VORBIS;
			default:				return Av::Format::OGG_OPUS;
			}
		}

		struct StreamParameters
		{
			Av::InputFileParameters inputFileParameters;
			std::optional<Av::TranscodeParameters> transcodeParameters;
			bool estimateContentLength{};
		};

		StreamParameters getStreamParameters(RequestContext& context)
		{
			// Mandatory params
			const TrackId id{ getMandatoryParameterAs<TrackId>(context.parameters, "id") };

			// Optional params
			std::optional<std::size_t> maxBitRate{ getParameterAs<std::size_t>(context.parameters, "maxBitRate") };
			std::optional<std::string> format{ getParameterAs<std::string>(context.parameters, "format") };
			bool estimateContentLength{ getParameterAs<bool>(context.parameters, "estimateContentLength").value_or(false) };

			StreamParameters parameters;

			parameters.estimateContentLength = estimateContentLength;

			auto transaction{ context.dbSession.createSharedTransaction() };

			{
				auto track{ Track::find(context.dbSession, id) };
				if (!track)
					throw RequestedDataNotFoundError{};

				parameters.inputFileParameters.trackPath = track->getPath();
				parameters.inputFileParameters.duration = track->getDuration();
			}

			{
				const User::pointer user{ User::find(context.dbSession, context.userId) };
				if (!user)
					throw UserNotAuthorizedError{};

				// format = "raw" => no transcode. Other format values will be ignored
				const bool transcode{ (!format || (*format != "raw")) && user->getSubsonicTranscodeEnable() };
				if (transcode)
				{
					std::size_t bitRate{ user->getSubsonicTranscodeBitrate() / 1000 };

					// "If set to zero, no limit is imposed"
					if (maxBitRate && *maxBitRate != 0)
						bitRate = Utils::clamp(*maxBitRate, std::size_t{ 48 }, bitRate);

					Av::TranscodeParameters transcodeParameters;

					transcodeParameters.bitrate = bitRate * 1000;
					transcodeParameters.format = userTranscodeFormatToAvFormat(user->getSubsonicTranscodeFormat());
					transcodeParameters.stripMetadata = false; // We want clients to use metadata (offline use, replay gain, etc.)

					parameters.transcodeParameters = std::move(transcodeParameters);
				}
			}

			return parameters;
		}
	}

	void handleDownload(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
	{
		std::shared_ptr<IResourceHandler> resourceHandler;

		Wt::Http::ResponseContinuation* continuation{ request.continuation() };
		if (!continuation)
		{
			// Mandatory params
			Database::TrackId id{ getMandatoryParameterAs<Database::TrackId>(context.parameters, "id") };

			std::filesystem::path trackPath;
			{
				auto transaction{ context.dbSession.createSharedTransaction() };

				auto track{ Track::find(context.dbSession, id) };
				if (!track)
					throw RequestedDataNotFoundError{};

				trackPath = track->getPath();
			}

			resourceHandler = createFileResourceHandler(trackPath);
		}
		else
		{
			resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
		}

		continuation = resourceHandler->processRequest(request, response);
		if (continuation)
			continuation->setData(resourceHandler);
	}

	void handleStream(RequestContext& context, const Wt::Http::Request& request, Wt::Http::Response& response)
	{
		std::shared_ptr<IResourceHandler> resourceHandler;

		try
		{
			Wt::Http::ResponseContinuation* continuation = request.continuation();
			if (!continuation)
			{
				StreamParameters streamParameters{ getStreamParameters(context) };
				if (streamParameters.transcodeParameters)
					resourceHandler = Av::createTranscodeResourceHandler(streamParameters.inputFileParameters, *streamParameters.transcodeParameters, streamParameters.estimateContentLength);
				else
					resourceHandler = createFileResourceHandler(streamParameters.inputFileParameters.trackPath);
			}
			else
			{
				resourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(continuation->data());
			}

			continuation = resourceHandler->processRequest(request, response);
			if (continuation)
				continuation->setData(resourceHandler);
		}
		catch (const Av::Exception& e)
		{
			LMS_LOG(API_SUBSONIC, ERROR) << "Caught Av exception: " << e.what();
		}
	}

	void handleGetCoverArt(RequestContext& context, const Wt::Http::Request& /*request*/, Wt::Http::Response& response)
	{
		// Mandatory params
		const auto trackId{ getParameterAs<TrackId>(context.parameters, "id") };
		const auto releaseId{ getParameterAs<ReleaseId>(context.parameters, "id") };

		if (!trackId && !releaseId)
			throw BadParameterGenericError{ "id" };

		std::size_t size{ getParameterAs<std::size_t>(context.parameters, "size").value_or(1024) };
		size = ::Utils::clamp(size, std::size_t{ 32 }, std::size_t{ 2048 });

		std::shared_ptr<Image::IEncodedImage> cover;
		if (trackId)
			cover = Service<Cover::ICoverService>::get()->getFromTrack(*trackId, size);
		else if (releaseId)
			cover = Service<Cover::ICoverService>::get()->getFromRelease(*releaseId, size);

		response.out().write(reinterpret_cast<const char*>(cover->getData()), cover->getDataSize());
		response.setMimeType(std::string{ cover->getMimeType() });
	}

} // namespace API::Subsonic
