/*
 * Copyright (C) 2013 Emeric Poupon
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


#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cassert>

#include "database/SqlQuery.hpp"

int main(void)
{
	try {
		SqlQuery query;


		query.select("artist.name").And("track.name");
		query.from().And( FromClause("artist") ).And( FromClause("track"));
		query.where().And( WhereClause("artist.id = track.artist_id") );

		std::cout << "Query = '" << query.get() << "'" << std::endl;

		assert(query.get() == "SELECT artist.name,track.name FROM artist,track WHERE (artist.id = track.artist_id)");

		{
			WhereClause clause;

			clause.Or(WhereClause("artist.name = ?").bind("Sepultura1"));
			clause.Or(WhereClause("artist.name = ?").bind("Sepultura2"));
			clause.Or(WhereClause("artist.name = ?")).bind("Sepultura3");

			query.where().And(clause);

			assert(query.get() == "SELECT artist.name,track.name FROM artist,track WHERE (artist.id = track.artist_id) AND ((artist.name = ?) OR (artist.name = ?) OR (artist.name = ?))");

			assert(query.where().getBindArgs().size() == 3);
			std::cout << "Query = '" << query.get() << "'" << std::endl;
		}

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

