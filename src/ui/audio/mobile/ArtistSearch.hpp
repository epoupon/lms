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

#ifndef UI_MOBILE_ARTIST_SEARCH_HPP
#define UI_MOBILE_ARTIST_SEARCH_HPP

#include <boost/algorithm/string/split.hpp>
#include <Wt/WContainerWidget>

#include "database/DatabaseHandler.hpp"

namespace UserInterface {
namespace Mobile {

class ArtistSearch : public Wt::WContainerWidget
{
	public:

		ArtistSearch(Database::Handler& db, Wt::WContainerWidget *parent = 0);

		void search(Database::SearchFilter filter, std::size_t nb);

		// Slots
		Wt::Signal<std::string>&	artistSelected() { return _sigArtistSelected;}
		Wt::Signal<void>&		moreArtistsSelected() { return _sigMoreArtistsSelected;}

	private:

		Wt::Signal<std::string> _sigArtistSelected;
		Wt::Signal<void>	_sigMoreArtistsSelected;

		void clear(void);
		void addResults(Database::SearchFilter filter, size_t nb);

		Database::Handler&	_db;
		std::size_t		_resCount;
};

} // namespace Mobile
} // namespace UserInterface

#endif
