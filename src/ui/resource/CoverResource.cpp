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

#include <boost/foreach.hpp>

#include <Wt/WApplication>
#include <Wt/Http/Response>

#include "logger/Logger.hpp"

#include "cover/CoverArtGrabber.hpp"

#include "CoverResource.hpp"

namespace UserInterface {

CoverResource::CoverResource(Database::Handler& db, std::size_t size, Wt::WObject *parent)
:  Wt::WResource(parent),
_db(db),
_size(size)
{
	// Load default cover art

	std::vector<unsigned char> data;

	{
		std::ifstream ist(Wt::WApplication::instance()->docRoot() + "/images/unknown-cover.jpg");
		char c;
		while(ist.get(c))
			data.push_back(c);
	}

	_defaultCover.setData(data);
	_defaultCover.setMimeType("image/jpeg");

	_defaultCover.scale(size);
}

CoverResource:: ~CoverResource()
{
	beingDeleted();
}

std::string
CoverResource::getReleaseUrl(std::string releaseName)
{
	return url() +  "&release=" + releaseName;
}

std::string
CoverResource::getTrackUrl(Database::Track::id_type trackId)
{
	return url()+ "&trackid=" + std::to_string(trackId);
}

void
CoverResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	// Get the id of the track
	const std::string *trackIdStr = request.getParameter("trackid");
	const std::string *releaseStr = request.getParameter("release");

	std::vector<CoverArt::CoverArt> covers;

	if (trackIdStr)
	{
		Database::Track::id_type trackId;
		{
			std::istringstream iss(*trackIdStr);
			iss >> trackId;
		}

		Wt::Dbo::Transaction transaction(_db.getSession());

		Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
		covers = CoverArt::Grabber::getFromTrack(track);

		transaction.commit();
	}
	else if (releaseStr)
	{
		Wt::Dbo::Transaction transaction(_db.getSession());
		covers = CoverArt::Grabber::getFromRelease(_db.getSession(), *releaseStr);
	}

	BOOST_FOREACH(CoverArt::CoverArt& cover, covers)
	{
		if (cover.scale(_size))
		{
			response.setMimeType( cover.getMimeType() );

			BOOST_FOREACH(unsigned char c, cover.getData())
				response.out().put( c );

			return;
		}
	}

	// If no cover found, just send default one
	response.setMimeType( _defaultCover.getMimeType() );
	BOOST_FOREACH(unsigned char c, _defaultCover.getData())
		response.out().put( c );

}

} // namespace UserInterface
