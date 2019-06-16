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

#include <Wt/Dbo/WtSqlTraits.h>

#include "TrackArtistLink.hpp"
#include "Types.hpp"

namespace Database
{

class Artist;
class Cluster;
class ClusterType;
class Release;
class Track;
class User;

class Release : public Wt::Dbo::Dbo<Release>
{
	public:

		using pointer = Wt::Dbo::ptr<Release>;

		Release() {}
		Release(const std::string& name, const std::string& MBID = "");

		// Accessors
		static std::size_t		getCount(Wt::Dbo::Session& session);
		static pointer			getByMBID(Wt::Dbo::Session& session, const std::string& MBID);
		static std::vector<pointer>	getByName(Wt::Dbo::Session& session, const std::string& name);
		static pointer			getById(Wt::Dbo::Session& session, IdType id);
		static std::vector<pointer>	getAllOrphans(Wt::Dbo::Session& session); // no track related
		static std::vector<pointer>	getAll(Wt::Dbo::Session& session, boost::optional<std::size_t> offset = {}, boost::optional<std::size_t> size = {});
		static std::vector<pointer>	getAllRandom(Wt::Dbo::Session& session, boost::optional<std::size_t> size = {});
		static std::vector<pointer>	getLastAdded(Wt::Dbo::Session& session, Wt::WDateTime after, boost::optional<std::size_t> offset = {}, boost::optional<std::size_t> size = {});

		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session, const std::set<IdType>& clusters);
		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session,
							const std::set<IdType>& clusters,           // at least one track that belongs to these clusters
							const std::vector<std::string> keywords,        // name must match all of these keywords
							boost::optional<std::size_t> offset,
							boost::optional<std::size_t> size,
							bool& moreExpected);

		std::vector<Wt::Dbo::ptr<Track>> getTracks(const std::set<IdType>& clusters = std::set<IdType>()) const;

		// Get the cluster of the tracks that belong to this release
		// Each clusters are grouped by cluster type, sorted by the number of occurence
		// size is the max number of cluster per cluster type
		std::vector<std::vector<Wt::Dbo::ptr<Cluster>>> getClusterGroups(std::vector<Wt::Dbo::ptr<ClusterType>> clusterTypes, std::size_t size) const;

		// Create
		static pointer create(Wt::Dbo::Session& session, const std::string& name, const std::string& MBID = "");

		// Utility functions
		boost::optional<int> getReleaseYear(bool originalDate = false) const; // 0 if unknown or various
		boost::optional<std::string> getCopyright() const;
		boost::optional<std::string> getCopyrightURL() const;

		// Modifiers
		void setTotalDiscNumber(std::size_t num)	{ _totalDiscNumber = static_cast<int>(num); }
		void setTotalTrackNumber(std::size_t num)	{ _totalTrackNumber = static_cast<int>(num); }

		// Accessors
		std::string			getName() const		{ return _name; }
		std::string			getMBID() const		{ return _MBID; }
		boost::optional<std::size_t>	getTotalTrackNumber() const;
		boost::optional<std::size_t>	getTotalDiscNumber() const;
		std::chrono::milliseconds	getDuration() const;

		// Get the artists of this release
		std::vector<Wt::Dbo::ptr<Artist> > getArtists(TrackArtistLink::Type type = TrackArtistLink::Type::Artist) const;
		std::vector<Wt::Dbo::ptr<Artist> > getReleaseArtists() const { return getArtists(TrackArtistLink::Type::ReleaseArtist); }
		bool hasVariousArtists() const;

		void setMBID(std::string mbid) { _MBID = mbid; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name, "name");
				Wt::Dbo::field(a, _MBID, "mbid");
				Wt::Dbo::field(a, _totalDiscNumber,	"total_disc_number");
				Wt::Dbo::field(a, _totalTrackNumber,	"total_track_number");

				Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToOne, "release");
				Wt::Dbo::hasMany(a, _starringUsers, Wt::Dbo::ManyToMany, "user_release_starred", "", Wt::Dbo::OnDeleteCascade);
			}

	private:
		static const std::size_t _maxNameLength {128};

		std::string	_name;
		std::string	_MBID;
		int		_totalDiscNumber {};
		int		_totalTrackNumber {};

		Wt::Dbo::collection<Wt::Dbo::ptr<Track>>	_tracks; // Tracks in the release
		Wt::Dbo::collection<Wt::Dbo::ptr<User>>		_starringUsers; // Users that starred this release
};

} // namespace Database


