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

#include "database/DatabaseHandler.hpp"

static const std::string trackMBID = "123e4567-e89b-12d3-a456-426655440000";
static const std::string artistMBID = "xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx";
static const std::string releaseMBID = "xxxxxxxx-xxxx-9877-Nxxx-xxxxxxxxxxxx";

int main(void)
{
	try
	{
		using namespace Database;

		boost::filesystem::remove("test.db");

		Handler db("test.db");

		// Create
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			Track::pointer track = Track::create(db.getSession(), "test.mp2");

			track.modify()->setName("track01");
			track.modify()->setMBID(trackMBID);

			Artist::pointer artist = Artist::create(db.getSession(), "artist01", artistMBID);
			Release::pointer release = Release::create(db.getSession(), "release01", releaseMBID);
			Genre::pointer genre = Genre::create(db.getSession(), "genre01");

			track.modify()->setArtist(artist);
			track.modify()->setRelease(release);
			track.modify()->setGenres( { genre });
		}

		// Search
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			Track::pointer track = Track::getByMBID(db.getSession(), trackMBID);
			assert(track);
			assert(track->getArtist()->getMBID() == artistMBID);

			Track::pointer trackNotFound = Track::getByMBID(db.getSession(), "foobar");
			assert(!trackNotFound);

			Artist::pointer artist = Artist::getByMBID(db.getSession(), artistMBID);
			assert(artist);
		}

		// Search Filters
		// Select track by track name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track, {"track"}}}});
			std::vector<Track::pointer> res = Track::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);

			filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track,{"not-found"}}}});
			res = Track::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 0 );
		}

		// Select track by artist name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Artist, {"artist"}}}});
			std::vector<Track::pointer> res = Track::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
		}

		// Select track by artist id
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::IdMatch({{SearchFilter::Field::Artist, {1}}});
			std::vector<Track::pointer> res = Track::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
		}

		// Select track by track name + artist id
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter;
			filter.idMatch[SearchFilter::Field::Artist] = { 1 };
			filter.nameLikeMatch = {{{ SearchFilter::Field::Track, {"track"} }}};
			std::vector<Track::pointer> res = Track::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
			assert(res.front()->getName() == "track01" );
		}

		// Select track by genre name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Genre, {"genre"}}}});
			std::vector<Track::pointer> res = Track::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
			assert(res.front()->getName() == "track01" );
		}

		// Select artist by track name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track, { "track" } }}});
			std::vector<Artist::pointer> res = Artist::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);

			filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track, {"badtrack"} }}});
			res = Artist::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 0);
		}

		// Select artist by track id
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::IdMatch({{SearchFilter::Field::Track, {1} }});
			std::vector<Artist::pointer> res = Artist::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
		}

		// Select Artist by name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Artist, {"artist"}}}});
			std::vector<Artist::pointer> res = Artist::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
		}

		// Select Release by name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Release, {"release"} }}});

			std::vector<Release::pointer> res = Release::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
		}

		// Select Release by track name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track, {"track"}}}});
			std::vector<Release::pointer> res = Release::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
			assert(res.front()->getName() == "release01");
		}

		// Select genre by name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Genre, {"genre"}}}});
			std::vector<Genre::pointer> res = Genre::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
			assert(res.front()->getName() == "genre01");
		}

		// Select genre by track name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track, {"track"}}}});
			std::vector<Genre::pointer> res = Genre::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
			assert(res.front()->getName() == "genre01");
		}

		// Select genre by track name and artist name
		{
			Wt::Dbo::Transaction transaction(db.getSession());

			SearchFilter filter = SearchFilter::NameLikeMatch({{{SearchFilter::Field::Track, {"track"}},
					{SearchFilter::Field::Artist, {"artist"}}}});
			std::vector<Genre::pointer> res = Genre::getByFilter(db.getSession(), filter, -1, -1);
			assert(res.size() == 1);
			assert(res.front().id() == 1);
			assert(res.front()->getName() == "genre01");
		}

	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

