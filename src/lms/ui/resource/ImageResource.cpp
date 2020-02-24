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

namespace UserInterface {

ImageResource::ImageResource()
{
	setTakesUpdateLock(true);
}

ImageResource::~ImageResource()
{
	beingDeleted();
}

std::string
ImageResource::getReleaseUrl(Database::IdType releaseId, std::size_t size) const
{
	return url() + "&releaseid=" + std::to_string(releaseId) + "&size=" + std::to_string(size);
}

std::string
ImageResource::getTrackUrl(Database::IdType trackId, std::size_t size) const
{
	return url() + "&trackid=" + std::to_string(trackId) + "&size=" + std::to_string(size);
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
		return;

	const auto size {StringUtils::readAs<std::size_t>(*sizeStr)};
	if (!size || *size > maxSize)
		return;

	std::vector<uint8_t> cover;

	if (trackIdStr)
	{
		const auto trackId {StringUtils::readAs<Database::IdType>(*trackIdStr)};
		if (!trackId)
			return;

		cover = Service<CoverArt::IGrabber>::get()->getFromTrack(LmsApp->getDbSession(), *trackId, CoverArt::Format::JPEG, *size);
	}
	else if (releaseIdStr)
	{
		const auto releaseId {StringUtils::readAs<Database::IdType>(*releaseIdStr)};
		if (!releaseId)
			return;

		cover = Service<CoverArt::IGrabber>::get()->getFromRelease(LmsApp->getDbSession(), *releaseId, CoverArt::Format::JPEG, *size);
	}
	else
		return;

	response.setMimeType(getMimeType());

	response.out().write(reinterpret_cast<const char *>(&cover[0]), cover.size());
}

std::string
ImageResource::getMimeType()
{
	return CoverArt::formatToMimeType(CoverArt::Format::JPEG);
}

} // namespace UserInterface
