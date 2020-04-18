/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "ReleaseLink.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WText.h>

#include "database/Release.hpp"
#include "resource/ImageResource.hpp"

#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

ReleaseLink::ReleaseLink(Database::Release::pointer release)
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.ReleaseLink.template"))
{
	bindWidget("release-name", LmsApplication::createReleaseAnchor(release));

	Wt::WAnchor* anchor = bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
	auto cover = std::make_unique<Wt::WImage>();
	cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), 64));
	cover->setStyleClass("Lms-cover-smaller");
	anchor->setImage(std::move(cover));

	auto artists = release->getReleaseArtists();
	if (artists.empty())
		artists = release->getArtists();

	if (artists.size() > 1)
	{
		setCondition("if-has-artist", true);
		bindNew<Wt::WText>("artist-name", Wt::WString::tr("Lms.Explore.various-artists"));
	}
	else if (artists.size() == 1)
	{
		setCondition("if-has-artist", true);
		bindWidget("artist-name", LmsApplication::createArtistAnchor(artists.front()));
	}
}

} // namespace UserInterface

