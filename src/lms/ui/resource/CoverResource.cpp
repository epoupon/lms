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

#include "CoverResource.hpp"

#include <Wt/WApplication.h>
#include <Wt/Http/Response.h>

#include "services/cover/ICoverService.hpp"
#include "services/database/Track.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"

#include "LmsApplication.hpp"

#define LOG(level)	LMS_LOG(UI, level) << "Image resource: "

namespace UserInterface {

CoverResource::CoverResource()
{
	LmsApp->getScannerEvents().scanComplete.connect(this, [this](const Scanner::ScanStats& stats)
	{
		if (stats.nbChanges())
			setChanged();
	});
}

CoverResource::~CoverResource()
{
	beingDeleted();
}

std::string
CoverResource::getReleaseUrl(Database::ReleaseId releaseId, Size size) const
{
	return url() + "&releaseid=" + releaseId.toString() + "&size=" + std::to_string(static_cast<std::size_t>(size));
}

std::string
CoverResource::getTrackUrl(Database::TrackId trackId, Size size) const
{
	return url() + "&trackid=" + trackId.toString() + "&size=" + std::to_string(static_cast<std::size_t>(size));
}

void
CoverResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
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

	std::shared_ptr<Image::IEncodedImage> cover;

	if (trackIdStr)
	{
		LOG(DEBUG) << "Requested cover for track " << *trackIdStr << ", size = " << *size;

		const std::optional<Database::TrackId> trackId {StringUtils::readAs<Database::TrackId::ValueType>(*trackIdStr)};
		if (!trackId)
		{
			LOG(DEBUG) << "track not found";
			return;
		}

		cover = Service<Cover::ICoverService>::get()->getFromTrack(*trackId, *size);
	}
	else if (releaseIdStr)
	{
		LOG(DEBUG) << "Requested cover for release " << *releaseIdStr << ", size = " << *size;

		const std::optional<Database::ReleaseId> releaseId {StringUtils::readAs<Database::ReleaseId::ValueType>(*releaseIdStr)};
		if (!releaseId)
			return;

		cover = Service<Cover::ICoverService>::get()->getFromRelease(*releaseId, *size);
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
