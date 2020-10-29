/*
 * Copyright (C) 2014 Emeric Poupon
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

#include "ImageResource.hpp"

#include <Wt/WApplication.h>
#include <Wt/Http/Response.h>

#include "cover/ICoverArtGrabber.hpp"
#include "database/Track.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "LmsApplication.hpp"

#define LOG(level)	LMS_LOG(UI, level) << "Image resource: "

namespace UserInterface {

ImageResource::~ImageResource()
{
	beingDeleted();
}

std::string
ImageResource::getReleaseUrl(Database::IdType releaseId, Size size) const
{
	return url() + "&releaseid=" + std::to_string(releaseId) + "&size=" + std::to_string(static_cast<std::size_t>(size));
}

std::string
ImageResource::getTrackUrl(Database::IdType trackId, Size size) const
{
	return url() + "&trackid=" + std::to_string(trackId) + "&size=" + std::to_string(static_cast<std::size_t>(size));
}

void
ImageResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	// Retrieve parameters
	const std::string *trackIdStr = request.getParameter("trackid");
	const std::string *releaseIdStr = request.getParameter("releaseid");
	const std::string *sizeStr = request.getParameter("size");

	// Mandatory parameter size
	if (!sizeStr)
	{
		LOG(DEBUG) << "no size provided!";
		return;
	}

	const auto size {StringUtils::readAs<std::size_t>(*sizeStr)};
	if (!size || *size > maxSize)
	{
		LOG(DEBUG) << "invalid size provided!";
		return;
	}

	std::shared_ptr<CoverArt::IEncodedImage> cover;

	if (trackIdStr)
	{
		LOG(DEBUG) << "Requested cover for track " << *trackIdStr << ", size = " << *size;

		const auto trackId {StringUtils::readAs<Database::IdType>(*trackIdStr)};
		if (!trackId)
		{
			LOG(DEBUG) << "track not found";
			return;
		}

		cover = Service<CoverArt::IGrabber>::get()->getFromTrack(LmsApp->getDbSession(), *trackId, *size);
	}
	else if (releaseIdStr)
	{
		LOG(DEBUG) << "Requested cover for release " << *releaseIdStr << ", size = " << *size;

		const auto releaseId {StringUtils::readAs<Database::IdType>(*releaseIdStr)};
		if (!releaseId)
			return;

		cover = Service<CoverArt::IGrabber>::get()->getFromRelease(LmsApp->getDbSession(), *releaseId, *size);
	}
	else
	{
		LOG(DEBUG) << "No track or release provided";
		return;
	}

	response.setMimeType(std::string {cover->getMimeType()});

	response.out().write(reinterpret_cast<const char *>(cover->getData()), cover->getDataSize());
}

} // namespace UserInterface
