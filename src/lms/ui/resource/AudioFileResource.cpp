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

#include "AudioFileResource.hpp"

#include <fstream>
#include <Wt/Http/Response.h>

#include "av/IAudioFile.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/FileResourceHandlerCreator.hpp"
#include "utils/Logger.hpp"
#include "utils/String.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

#define LOG(level)	LMS_LOG(UI, level) << "Audio file resource: "

AudioFileResource:: ~AudioFileResource()
{
	beingDeleted();
}

std::string
AudioFileResource::getUrl(Database::TrackId trackId) const
{
	return url()+ "&trackid=" + trackId.toString();
}

static
std::optional<std::filesystem::path>
getTrackPathFromTrackId(Database::TrackId trackId)
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Track::pointer track {Database::Track::find(LmsApp->getDbSession(), trackId)};
	if (!track)
	{
		LOG(ERROR) << "Missing track";
		return std::nullopt;
	}

	return track->getPath();
}

static
std::optional<std::filesystem::path>
getTrackPathFromURLArgs(const Wt::Http::Request& request)
{
	const std::string* trackIdParameter {request.getParameter("trackid")};
	if (!trackIdParameter)
	{
		LOG(ERROR) << "Missing trackid URL parameter!";
		return std::nullopt;
	}

	const std::optional<Database::TrackId> trackId {StringUtils::readAs<Database::TrackId::ValueType>(*trackIdParameter)};
	if (!trackId)
	{
		LOG(ERROR) << "Bad trackid URL parameter!";
		return std::nullopt;
	}

	return getTrackPathFromTrackId(*trackId);
}

void
AudioFileResource::handleRequest(const Wt::Http::Request& request,
		Wt::Http::Response& response)
{
	std::shared_ptr<IResourceHandler> fileResourceHandler;

	if (!request.continuation())
	{
		auto trackPath {getTrackPathFromURLArgs(request)};
		if (!trackPath)
			return;

		fileResourceHandler = createFileResourceHandler(*trackPath);

		const auto guessedAudioFormat {Av::guessAudioFileFormat(*trackPath)};
		if (guessedAudioFormat)
		{
			LOG(DEBUG) << "Set mime type to " << guessedAudioFormat->mimeType;
			response.setMimeType(guessedAudioFormat->mimeType);
		}
		else
			response.setMimeType("application/octet-stream");
	}
	else
	{
		fileResourceHandler = Wt::cpp17::any_cast<std::shared_ptr<IResourceHandler>>(request.continuation()->data());
	}

	auto* continuation {fileResourceHandler->processRequest(request, response)};
	if (continuation)
		continuation->setData(fileResourceHandler);
}


} // namespace UserInterface


