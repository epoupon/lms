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

#include <Wt/WApplication>
#include <Wt/WText>

#include "logger/Logger.hpp"
#include "SearchUtils.hpp"

#include "ArtistSearch.hpp"
#include "ReleaseSearch.hpp"
#include "TrackSearch.hpp"

#include "PreviewSearchView.hpp"

namespace UserInterface {
namespace Mobile {

#define SEARCH_NB_ITEMS	4

using namespace Database;

PreviewSearchView::PreviewSearchView(PlayQueueEvents& events, Wt::WContainerWidget* parent)
{
	const std::string pathPrefix = "/audio/search/preview";

	ArtistSearch* artistSearch = new ArtistSearch("Artists", this);
	ReleaseSearch* releaseSearch = new ReleaseSearch(this);
	TrackSearch* trackSearch = new TrackSearch(events, this);

	artistSearch->showMore().connect(std::bind([=]
	{
		std::string path = wApp->internalPath();
		path.replace(0, pathPrefix.length(), "/audio/search/artist");

		wApp->setInternalPath(path, true);
	}));

	releaseSearch->showMore().connect(std::bind([=]
	{
		std::string path = wApp->internalPath();
		path.replace(0, pathPrefix.length(), "/audio/search/release");

		wApp->setInternalPath(path, true);
	}));

	trackSearch->showMore().connect(std::bind([=]
	{
		std::string path = wApp->internalPath();
		path.replace(0, pathPrefix.length(), "/audio/search/track");

		wApp->setInternalPath(path, true);
	}));

	wApp->internalPathChanged().connect(std::bind([=] (std::string path)
	{
		if (!wApp->internalPathMatches(pathPrefix))
			return;

		std::vector<std::string> keywords = searchPathToSearchKeywords(path.substr(pathPrefix.length()));

		artistSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Artist, keywords), SEARCH_NB_ITEMS);
		releaseSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Release, keywords), SEARCH_NB_ITEMS, "Releases");
		trackSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Track, keywords), SEARCH_NB_ITEMS);

	}, std::placeholders::_1));
}

} // namespace Mobile
} // namespace UserInterface

