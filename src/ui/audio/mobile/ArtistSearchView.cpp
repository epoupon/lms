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

#include "ArtistSearchView.hpp"

namespace UserInterface {
namespace Mobile {

#define SEARCH_NB_ITEMS	20

using namespace Database;

ArtistSearchView::ArtistSearchView(Wt::WContainerWidget* parent)
{
	ArtistSearch* artistSearch = new ArtistSearch("Artists", this);
	artistSearch->showMore().connect(std::bind([=] {
		artistSearch->addResults(SEARCH_NB_ITEMS);
	}));

	wApp->internalPathChanged().connect(std::bind([=] (std::string path)
	{
		const std::string pathPrefix = "/audio/search/artist";

		if (!wApp->internalPathMatches(pathPrefix))
			return;

		std::vector<std::string> keywords = searchPathToSearchKeywords(path.substr(pathPrefix.length()));

		artistSearch->search(SearchFilter::ByNameAnd(SearchFilter::Field::Artist, keywords), SEARCH_NB_ITEMS);

	}, std::placeholders::_1));
}

} // namespace Mobile
} // namespace UserInterface

