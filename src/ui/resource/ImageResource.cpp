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

#include "cover/CoverArtGrabber.hpp"
#include "database/Track.hpp"
#include "main/Services.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

static const std::string unknownCoverPath = "/images/unknown-cover.jpg";
static const std::string unknownArtistImagePath = "/images/unknown-artist.jpg";

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

	auto size = readAs<std::size_t>(*sizeStr);
	if (!size || *size > maxSize)
		return;

	std::vector<uint8_t> cover;

	if (trackIdStr)
	{
		auto trackId = readAs<Database::IdType>(*trackIdStr);
		if (!trackId)
			return;

		// transactions are not thread safe
		{
			Wt::WApplication::UpdateLock lock(LmsApp);
			cover = getServices().coverArtGrabber->getFromTrack(LmsApp->getDboSession(), *trackId, Image::Format::JPEG, *size);
		}
	}
	else if (releaseIdStr)
	{
		auto releaseId = readAs<Database::IdType>(*releaseIdStr);
		if (!releaseId)
			return;

		// transactions are not thread safe
		{
			Wt::WApplication::UpdateLock lock(LmsApp);
			cover = getServices().coverArtGrabber->getFromRelease(LmsApp->getDboSession(), *releaseId, Image::Format::JPEG, *size);
		}
	}
	else
		return;

	response.setMimeType( Image::format_to_mimeType(Image::Format::JPEG) );
	response.out().write(reinterpret_cast<const char *>(&cover[0]), cover.size());
}

} // namespace UserInterface
