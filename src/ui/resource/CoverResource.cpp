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

#include <Wt/Http/Response>

#include "logger/Logger.hpp"

#include "cover/CoverArtGrabber.hpp"

#include "CoverResource.hpp"

namespace UserInterface {

CoverResource::CoverResource(const boost::filesystem::path& path, std::size_t size, Wt::WObject *parent)
:  Wt::WResource(parent),
_path(path),
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
	if (_data.empty())
	{
		std::vector<CoverArt::CoverArt> covers = CoverArt::Grabber::getFromTrack( _path );

		BOOST_FOREACH(CoverArt::CoverArt& cover, covers)
		{
			cover.scale(_size);
			_data = cover.getData();
			_mimeType = cover.getMimeType();
			break;
		}

		// TODO if not found fallback on an empty image
	}

	response.setMimeType(_mimeType);

	for (unsigned int i = 0; i < _data.size(); ++i)
	        response.out().put(_data[i]);
}


} // namespace UserInterface
