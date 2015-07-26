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

#include "cover/CoverArtGrabber.hpp"

#include "CoverResource.hpp"

namespace UserInterface {

const std::string CoverResource::unknownCoverPath = "/images/unknown-cover.jpg";

CoverResource::CoverResource(Database::Handler& db, Wt::WObject *parent)
:  Wt::WResource(parent),
_db(db)
{
}

CoverResource:: ~CoverResource()
{
	beingDeleted();
}

const CoverArt::CoverArt&
CoverResource::getDefaultCover(std::size_t size)
{
	auto itCover = _defaultCovers.find(size);
	if (itCover == _defaultCovers.end())
	{
		// Load default cover art for this size

		CoverArt::CoverArt defaultCover;

		std::vector<unsigned char> data;
		{
			std::ifstream ist(Wt::WApplication::instance()->docRoot() + unknownCoverPath);
			char c;
			while(ist.get(c))
				data.push_back(c);
		}

		defaultCover.setData(data);
		defaultCover.setMimeType("image/jpeg");
		defaultCover.scale(size);

		auto res = _defaultCovers.insert(std::make_pair(size, defaultCover));
		itCover = res.first;
	}

	return itCover->second;
}

std::string
CoverResource::getReleaseUrl(Database::Release::id_type releaseId, std::size_t size) const
{
	return url() + "&releaseid=" + std::to_string(releaseId) + "&size=" + std::to_string(size);
}

std::string
CoverResource::getTrackUrl(Database::Track::id_type trackId, std::size_t size) const
{
	return url() + "&trackid=" + std::to_string(trackId) + "&size=" + std::to_string(size);
}

std::string
CoverResource::getUnknownTrackUrl(size_t size) const
{
	return url() + "&size=" + std::to_string(size);
}

void
CoverResource::putCover(Wt::Http::Response& response, const CoverArt::CoverArt& cover)
{
	response.setMimeType( cover.getMimeType() );
	BOOST_FOREACH(unsigned char c, cover.getData())
		response.out().put( c );
}

void
CoverResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	// Retrieve parameters
	const std::string *trackIdStr = request.getParameter("trackid");
	const std::string *releaseIdStr = request.getParameter("releaseid");
	const std::string *sizeStr = request.getParameter("size");

	try
	{
		std::vector<CoverArt::CoverArt> covers;

		// Mandatory parameter size
		if (!sizeStr)
			return;

		std::size_t size = std::stol(*sizeStr);
		if (size > maxSize)
			return;

		if (trackIdStr)
		{
			Database::Track::id_type trackId = std::stol(*trackIdStr);
			boost::filesystem::path path;
			Database::Track::CoverType coverType = Database::Track::CoverType::None;

			{
				// transactions are not thread safe
				std::unique_lock<std::mutex> lock(_mutex);

				Wt::Dbo::Transaction transaction(_db.getSession());

				Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
				if (track)
				{
					coverType = track->getCoverType();
					path = track->getPath();
				}
			}

			switch (coverType)
			{
				case Database::Track::CoverType::Embedded:
					covers = CoverArt::Grabber::instance().getFromTrack(path);
					break;

				case Database::Track::CoverType::ExternalFile:
					covers = CoverArt::Grabber::instance().getFromDirectory(path.parent_path());
					break;

				case Database::Track::CoverType::None:
					break;
			}
		}
		else if (releaseIdStr)
		{
			Database::Release::id_type releaseId = std::stol(*releaseIdStr); // TODO try catch
			// transactions are not thread safe
			std::unique_lock<std::mutex> lock(_mutex);
			Wt::Dbo::Transaction transaction(_db.getSession());

			covers = CoverArt::Grabber::instance().getFromRelease(_db.getSession(), releaseId);
		}

		for (CoverArt::CoverArt& cover : covers)
		{
			if (cover.scale(size))
			{
				putCover(response, cover);
				return;
			}
		}

		// If no cover found, just send default one
		putCover(response, getDefaultCover(size));

	}
	catch (std::invalid_argument& e)
	{
		LMS_LOG(MOD_UI, SEV_ERROR) << "Invalid argument: " << e.what();
	}
}

} // namespace UserInterface
