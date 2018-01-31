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

#pragma once

#include <boost/optional.hpp>

#include <Wt/Dbo/Dbo>

#include "SearchFilter.hpp"

namespace Database
{

class Track;
class Release;
class Artist;

class Release : public Wt::Dbo::Dbo<Release>
{
	public:

		typedef Wt::Dbo::ptr<Release> pointer;
		typedef Wt::Dbo::dbo_traits<Release>::IdType id_type;

		Release() {}
		Release(const std::string& name, const std::string& MBID = "");

		// Accessors
		static pointer			getByMBID(Wt::Dbo::Session& session, const std::string& MBID);
		static std::vector<pointer>	getByName(Wt::Dbo::Session& session, const std::string& name);
		static pointer			getById(Wt::Dbo::Session& session, id_type id);
		static pointer			getNone(Wt::Dbo::Session& session); // Special entry
		static std::vector<pointer>	getAllOrphans(Wt::Dbo::Session& session); // no track related
		static std::vector<pointer>	getAll(Wt::Dbo::Session& session, int offset, int size);

		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session,
							const std::vector<id_type>& clusters,           // at least one track that belongs to these clusters
							const std::vector<std::string> keywords,        // name must match all of these keywords
							int offset,
							int size,
							bool& moreExpected);

		std::vector<Wt::Dbo::ptr<Track>> getTracks(const std::vector<id_type>& clusters = std::vector<id_type>()) const;

		// Create
		static pointer create(Wt::Dbo::Session& session, const std::string& name, const std::string& MBID = "");

		// Utility functions
		boost::optional<int> getReleaseYear(bool originalDate = false) const; // 0 if unknown or various

		// Accessors
		std::string	getName() const		{ return _name; }
		std::string	getMBID() const		{ return _MBID; }
		bool		isNone(void) const;
		boost::posix_time::time_duration getDuration(void) const;

		// Get the artists of this release
		std::vector<Wt::Dbo::ptr<Artist> > getArtists() const;
		bool hasVariousArtists() const;

		void setMBID(std::string mbid) { _MBID = mbid; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name, "name");
				Wt::Dbo::field(a, _MBID, "mbid");

				Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
			}

	private:
		static const std::size_t _maxNameLength = 128;

		std::string _name;
		std::string _MBID;

		Wt::Dbo::collection< Wt::Dbo::ptr<Track> > _tracks; // Tracks in the release
};

} // namespace Database


