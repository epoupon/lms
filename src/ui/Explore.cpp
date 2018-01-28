/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <Wt/WStackedWidget>
#include <Wt/WTemplate>
#include <Wt/WText>

#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

#include "ArtistsView.hpp"
#include "ArtistView.hpp"
#include "ReleasesView.hpp"
#include "ReleaseView.hpp"
#include "TracksView.hpp"

#include "Explore.hpp"

namespace UserInterface {


enum IdxRoot
{
	IdxArtists = 0,
	IdxArtist,
	IdxReleases,
	IdxRelease,
	IdxTracks,
};

static void
handlePathChange(Wt::WStackedWidget* stack)
{
	static const std::map<std::string, int> indexes =
	{
		{ "/artists",		IdxArtists },
		{ "/artist",		IdxArtist },
		{ "/releases",		IdxReleases },
		{ "/release",		IdxRelease },
		{ "/tracks",		IdxTracks },
	};

	LMS_LOG(UI, DEBUG) << "Internal path changed to '" << wApp->internalPath() << "'";

	for (auto index : indexes)
	{
		if (wApp->internalPathMatches(index.first))
		{
			stack->setCurrentIndex(index.second);
			return;
		}
	}
}

Explore::Explore(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	auto container = new Wt::WTemplate(Wt::WString::tr("template-explore"), this);

	auto filters = new Filters();
	container->bindWidget("filters", filters);

	// Contents
	Wt::WStackedWidget* stack = new Wt::WStackedWidget();
	container->bindWidget("contents", stack);

	stack->addWidget(new Artists(filters));
	stack->addWidget(new Artist(filters));
	stack->addWidget(new Releases(/*filters*/));
	stack->addWidget(new Release(/*filters*/));
	stack->addWidget(new Tracks(/*filters*/));

	wApp->internalPathChanged().connect(std::bind([=]
	{
		handlePathChange(stack);
	}));

	handlePathChange(stack);
}
} // namespace UserInterface

