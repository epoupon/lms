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

#include <Wt/WApplication>
#include <Wt/Http/Response>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "LmsApplication.hpp"

#include "cover/CoverArtGrabber.hpp"

#include "ImageResource.hpp"

namespace UserInterface {

static const std::string unknownCoverPath = "/images/unknown-cover.jpg";
static const std::string unknownArtistImagePath = "/images/unknown-artist.jpg";

ImageResource::ImageResource(Database::Handler& db, Wt::WObject *parent)
:  Wt::WResource(parent),
_db(db)
{
}

ImageResource::~ImageResource()
{
	beingDeleted();
}

Image::Image
ImageResource::getDefaultCover(std::size_t size)
{
	std::unique_lock<std::mutex> lock(_mutex);

	auto itCover = _defaultCovers.find(size);
	if (itCover == _defaultCovers.end())
	{
		// Load default cover art for this size
		Image::Image image;

		if (!image.load( Wt::WApplication::instance()->docRoot() + unknownCoverPath ))
			throw std::runtime_error("Cannot read default cover file");

		image.scale(size);

		auto res = _defaultCovers.insert(std::make_pair(size, image));
		itCover = res.first;
	}

	return itCover->second;
}

Image::Image
ImageResource::getDefaultArtistImage(std::size_t size)
{
	std::unique_lock<std::mutex> lock(_mutex);

	auto itCover = _defaultArtistImages.find(size);
	if (itCover == _defaultArtistImages.end())
	{
		Image::Image image;

		if (!image.load( Wt::WApplication::instance()->docRoot() + unknownArtistImagePath))
			throw std::runtime_error("Cannot read default artist image file");

		image.scale(size);

		auto res = _defaultArtistImages.insert(std::make_pair(size, image));
		itCover = res.first;
	}

	return itCover->second;
}

std::string
ImageResource::getReleaseUrl(Database::Release::id_type releaseId, std::size_t size) const
{
	return url() + "&releaseid=" + std::to_string(releaseId) + "&size=" + std::to_string(size);
}

std::string
ImageResource::getTrackUrl(Database::Track::id_type trackId, std::size_t size) const
{
	return url() + "&trackid=" + std::to_string(trackId) + "&size=" + std::to_string(size);
}

std::string
ImageResource::getArtistUrl(Database::Artist::id_type artistId, std::size_t size) const
{
	return url() + "&artistid=" + std::to_string(artistId) + "&size=" + std::to_string(size);
}

std::string
ImageResource::getUnknownTrackUrl(size_t size) const
{
	return url() + "&size=" + std::to_string(size);
}

void
ImageResource::putImage(Wt::Http::Response& response, Image::Image cover)
{
	std::vector<unsigned char> data;

	cover.save(data, Image::Format::JPEG);

	response.setMimeType( Image::format_to_mimeType(Image::Format::JPEG) );
	response.out().write(reinterpret_cast<const char *>(&data[0]), data.size());
}

void
ImageResource::putCover(Wt::Http::Response& response, std::vector<Image::Image> covers, size_t size)
{
	for (Image::Image cover : covers)
	{
		if (cover.scale(size))
		{
			putImage(response, cover);
			return;
		}
	}

	// If no cover found, just send default one
	putImage(response, getDefaultCover(size));
}

void
ImageResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	// Retrieve parameters
	const std::string *trackIdStr = request.getParameter("trackid");
	const std::string *releaseIdStr = request.getParameter("releaseid");
	const std::string *artistIdStr = request.getParameter("artistid");
	const std::string *sizeStr = request.getParameter("size");

	// Mandatory parameter size
	if (!sizeStr)
		return;

	std::size_t size;
	if (!readAs(*sizeStr, size) || size > maxSize)
		return;

	if (trackIdStr)
	{
		Database::Track::id_type trackId;
		if (!readAs(*trackIdStr, trackId))
			return;

		boost::filesystem::path path;
		Database::Track::CoverType coverType = Database::Track::CoverType::None;
		{
			// transactions are not thread safe
			Wt::WApplication::UpdateLock lock(LmsApplication::instance());

			Wt::Dbo::Transaction transaction(_db.getSession());

			Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
			if (track)
			{
				coverType = track->getCoverType();
				path = track->getPath();
			}
		}

		std::vector<Image::Image> covers;
		switch (coverType)
		{
			case Database::Track::CoverType::Embedded:
				covers = CoverArt::Grabber::instance().getFromTrack(path);
				break;

			case Database::Track::CoverType::None:
				covers = CoverArt::Grabber::instance().getFromDirectory(path.parent_path());
				break;
		}

		putCover(response, covers, size);
		return;
	}
	else if (releaseIdStr)
	{
		Database::Release::id_type releaseId;
		if (!readAs(*releaseIdStr, releaseId))
			return;

		std::vector<Image::Image> covers;

		// transactions are not thread safe
		{
			Wt::WApplication::UpdateLock lock(LmsApplication::instance());
			covers = CoverArt::Grabber::instance().getFromRelease(_db.getSession(), releaseId);
		}

		putCover(response, covers, size);
		return;
	}
	else if (artistIdStr)
	{
		putImage(response, getDefaultArtistImage(size));
		return;
	}
	else
	{
		putImage(response, getDefaultCover(size));
		return;
	}
}

} // namespace UserInterface
