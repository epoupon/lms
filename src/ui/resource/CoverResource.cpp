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
}

CoverResource:: ~CoverResource()
{
	beingDeleted();
}


void
CoverResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	// Get the id of the track
	const std::string *trackIdStr = request.getParameter("coverid");

	if (trackIdStr)
	{
		Database::Track::id_type trackId;
		{
			std::istringstream iss(*trackIdStr); iss >> trackId;
		}

		Wt::Dbo::Transaction transaction(_db.getSession());

		Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
		std::vector<CoverArt::CoverArt> covers = CoverArt::Grabber::getFromTrack(track);

		transaction.commit();

		BOOST_FOREACH(CoverArt::CoverArt& cover, covers)
		{
			if (cover.scale(_size))
			{
				response.setMimeType( cover.getMimeType() );

				BOOST_FOREACH(unsigned char c, cover.getData())
					response.out().put( c );

				return;
			}
			else
				LMS_LOG(MOD_UI, SEV_DEBUG) << "Resize error for track id = " << trackId;
		}
		LMS_LOG(MOD_UI, SEV_DEBUG) << "no valid cover found for track id = " << trackId;
		{
			std::ifstream ist(Wt::WApplication::instance()->docRoot() + "/images/unknown-cover.jpg");

			char c;
			while(ist.get(c))
				response.out().put( c );

			response.setMimeType("image/jpeg");
		}
	}
	else
		LMS_LOG(MOD_UI, SEV_DEBUG) << "no cover id parameter";
}


} // namespace UserInterface
