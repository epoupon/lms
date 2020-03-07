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

#include "av/AvInfo.hpp"
#include "database/Track.hpp"
#include "utils/FileResourceHandler.hpp"
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
AudioFileResource::getUrl(Database::IdType trackId) const
{
	return url()+ "&trackid=" + std::to_string(trackId);
}

static
std::optional<std::filesystem::path>
getTrackPathFromTrackId(Database::IdType trackId)
{
	// DbSession are not thread safe
	Wt::WApplication::UpdateLock lock {LmsApp};

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	const Database::Track::pointer track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
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

	const auto trackId {StringUtils::readAs<Database::IdType>(*trackIdParameter)};
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
	std::optional<FileResourceHandler::ContinuationData> continuationData;
	if (!request.continuation())
	{
		auto trackPath {getTrackPathFromURLArgs(request)};
		if (!trackPath)
			return;

		continuationData = FileResourceHandler::handleInitialRequest(request, response, *trackPath);
	}
	else
	{
		auto currentContinuationData {Wt::cpp17::any_cast<FileResourceHandler::ContinuationData>(request.continuation()->data())};
		continuationData = FileResourceHandler::handleContinuationRequest(request, response, currentContinuationData);
	}

	if (continuationData)
	{
		auto* continuation {response.createContinuation()};
		continuation->setData(*continuationData);
	}

}

} // namespace UserInterface


