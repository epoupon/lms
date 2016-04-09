/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "LmsApplication.hpp"

#include "utils/Utils.hpp"

#include "ReleaseSearch.hpp"
#include "ArtistView.hpp"

namespace UserInterface {
namespace Mobile {

using namespace Database;

ArtistView::ArtistView(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	ReleaseSearch *releases = new ReleaseSearch(this);

	wApp->internalPathChanged().connect(std::bind([=] (std::string path)
	{
		const std::string pathPrefix = "/audio/artist/";

		if (!wApp->internalPathMatches(pathPrefix))
			return;

		std::string strId = path.substr(pathPrefix.length());

		Artist::id_type id;
		if (readAs(strId, id))
		{
			Wt::Dbo::Transaction transaction(DboSession());

			auto artist = Artist::getById(DboSession(), id);
			releases->search(SearchFilter::ById(SearchFilter::Field::Artist, id), 20, artist ? Wt::WString::fromUTF8(artist->getName()) : "Unknown artist");
		}

	}, std::placeholders::_1));
}

Wt::WLink
ArtistView::getLink(Database::Artist::id_type id)
{
	return Wt::WLink(Wt::WLink::InternalPath, "/audio/artist/" + std::to_string(id));
}

} //namespace Mobile
} //namespace UserInterface

