/*
 * Copyright (C) 2013-2016 Emeric Poupon
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

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

#include <Wt/WDateTime>

#include "SearchFilter.hpp"

namespace Database {


class Artist;
class Release;
class Track;
class PlaylistEntry;

class Cluster
{
	public:

		enum class Type
		{
			Genre	= 1,
			Mood	= 2,
		};

		typedef Wt::Dbo::ptr<Cluster> pointer;
		typedef Wt::Dbo::dbo_traits<Cluster>::IdType id_type;

		Cluster();
		Cluster(std::string type, std::string name);

		// Find utility
		static pointer get(Wt::Dbo::Session& session, std::string type, std::string name);
		static pointer getNone(Wt::Dbo::Session& session);
		static std::vector<pointer> getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset = -1, int size = -1);
		static Wt::Dbo::collection<pointer> getAll(Wt::Dbo::Session& session);
		static std::vector<std::string> getAllTypes(Wt::Dbo::Session& session);
		static std::vector<pointer> getByType(Wt::Dbo::Session& session, std::string type);

		// Create utility
		static pointer create(Wt::Dbo::Session& session, std::string type, std::string name);

		// Remove utility
		static void remove(Wt::Dbo::Session& session, std::string type); // nested transaction

		// Accessors
		const std::string& getName(void) const { return _name; }
		const std::string& getType(void) const { return _type; }
		bool isNone(void) const;
		const Wt::Dbo::collection< Wt::Dbo::ptr<Track> >&	getTracks() const { return _tracks;}

		void addTrack(Wt::Dbo::Session& session, Wt::Dbo::dbo_traits<Track>::IdType trackId);
		void addTrack(Wt::Dbo::ptr<Track> track) { _tracks.insert(track); }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name,	"name");
				Wt::Dbo::field(a, _type,	"type");
				Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
			}

	private:
		static Wt::Dbo::Query<pointer> getQuery(Wt::Dbo::Session& session, SearchFilter filter);

		static const std::size_t _maxNameLength = 128;
		static const std::size_t _maxTypeLength = 128;

		std::string	_type;
		std::string	_name;

		Wt::Dbo::collection< Wt::Dbo::ptr<Track> > _tracks;
};

class Track
{
	public:

		typedef Wt::Dbo::ptr<Track> pointer;
		typedef Wt::Dbo::dbo_traits<Track>::IdType id_type;

		Track() {}
		Track(const boost::filesystem::path& p);

		enum class CoverType
		{
			Embedded,	// Contains embedded cover
			None,		// No local cover available
		};

		// Find utility functions
		static pointer getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p);
		static pointer getById(Wt::Dbo::Session& session, id_type id);
		static pointer getByMBID(Wt::Dbo::Session& session, const std::string& MBID);
		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session,
							const std::set<id_type>& clusters,           // tracks that belong to these clusters
							const std::vector<std::string> keywords,        // name must match all of these keywords
							int offset,
							int size,
							bool& moreExpected);

		static Wt::Dbo::collection< pointer > getAll(Wt::Dbo::Session& session);
		static std::vector<id_type> getAllIds(Wt::Dbo::Session& session); // nested transaction
		static std::vector<boost::filesystem::path> getAllPaths(Wt::Dbo::Session& session); // nested transaction
		static std::vector<pointer> getMBIDDuplicates(Wt::Dbo::Session& session);
		static std::vector<pointer> getChecksumDuplicates(Wt::Dbo::Session& session);

		// Utility fonctions
		// Stats for a given search filter
		typedef boost::tuple<
				int,		// Total tracks
				boost::posix_time::time_duration // Total duration
				> StatsQueryResult;
		static StatsQueryResult getStats(Wt::Dbo::Session& session, SearchFilter filter);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, const boost::filesystem::path& p);

		// Remove utility
		static void removeClusters(std::string type);

		// Accessors
		void setTrackNumber(int num)					{ _trackNumber = num; }
		void setTotalTrackNumber(int num)				{ _totalTrackNumber = num; }
		void setDiscNumber(int num)					{ _discNumber = num; }
		void setTotalDiscNumber(int num)				{ _totalDiscNumber = num; }
		void setName(const std::string& name)				{ _name = std::string(name, 0, _maxNameLength); }
		void setDuration(boost::posix_time::time_duration duration)	{ _duration = duration; }
		void setLastWriteTime(boost::posix_time::ptime time)		{ _fileLastWrite = time; }
		void setAddedTime(boost::posix_time::ptime time)		{ _fileAdded = time; }
		void setChecksum(const std::vector<unsigned char>& checksum)	{ _fileChecksum = checksum; }
		void setDate(const boost::posix_time::ptime& date)		{ _date = date; }
		void setOriginalDate(const boost::posix_time::ptime& date)	{ _originalDate = date; }
		void setGenres(const std::string& genreList)			{ _genreList = genreList; }
		void setCoverType(CoverType coverType)				{ _coverType = coverType; }
		void setMBID(const std::string& MBID)				{ _MBID = MBID; }
		void setArtist(Wt::Dbo::ptr<Artist> artist)			{ _artist = artist; }
		void setRelease(Wt::Dbo::ptr<Release> release)			{ _release = release; }

		boost::optional<std::size_t>	getTrackNumber(void) const;
		boost::optional<std::size_t>	getTotalTrackNumber(void) const;
		boost::optional<std::size_t>	getDiscNumber(void) const;
		boost::optional<std::size_t>	getTotalDiscNumber(void) const;
		std::string 			getName(void) const			{ return _name; }
		boost::filesystem::path		getPath(void) const			{ return _filePath; }
		boost::posix_time::time_duration	getDuration(void) const		{ return _duration; }
		boost::posix_time::ptime	getDate(void) const			{ return _date; }
		boost::posix_time::ptime	getOriginalDate(void) const		{ return _originalDate; }
		boost::posix_time::ptime	getLastWriteTime(void) const		{ return _fileLastWrite; }
		boost::posix_time::ptime	getAddedTime(void) const		{ return _fileAdded; }
		const std::vector<unsigned char>& getChecksum(void) const		{ return _fileChecksum; }
		CoverType			getCoverType(void) const		{ return _coverType; }
		const std::string&		getMBID(void) const			{ return _MBID; }
		Wt::Dbo::ptr<Artist>		getArtist(void) const			{ return _artist; }
		Wt::Dbo::ptr<Release>		getRelease(void) const			{ return _release; }
		std::vector< Cluster::pointer >	getClusters(void) const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _trackNumber,		"track_number");
				Wt::Dbo::field(a, _totalTrackNumber,	"total_track_number");
				Wt::Dbo::field(a, _discNumber,		"disc_number");
				Wt::Dbo::field(a, _totalDiscNumber,	"total_disc_number");
				Wt::Dbo::field(a, _name,		"name");
				Wt::Dbo::field(a, _duration,		"duration");
				Wt::Dbo::field(a, _date,		"date");
				Wt::Dbo::field(a, _originalDate,	"original_date");
				Wt::Dbo::field(a, _genreList,		"genre_list");
				Wt::Dbo::field(a, _filePath,		"file_path");
				Wt::Dbo::field(a, _fileLastWrite,	"file_last_write");
				Wt::Dbo::field(a, _fileAdded,		"file_added");
				Wt::Dbo::field(a, _fileChecksum,	"checksum");
				Wt::Dbo::field(a, _coverType,		"cover_type");
				Wt::Dbo::field(a, _MBID,		"mbid");
				Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _playlistEntries, Wt::Dbo::ManyToOne, "track");
			}

	private:

		static const std::size_t _maxNameLength = 128;

		int					_trackNumber;
		int					_totalTrackNumber;
		int					_discNumber;
		int					_totalDiscNumber;
		std::string				_name;
		std::string				_artistName;
		std::string				_releaseName;
		boost::posix_time::time_duration	_duration;
		boost::posix_time::ptime		_date;
		boost::posix_time::ptime		_originalDate; // original date time
		std::string				_genreList;
		std::string				_filePath;
		std::vector<unsigned char>		_fileChecksum;
		boost::posix_time::ptime		_fileLastWrite;
		boost::posix_time::ptime		_fileAdded;
		CoverType				_coverType;
		std::string				_MBID; // Musicbrainz Identifier

		Wt::Dbo::ptr<Artist>			_artist;
		Wt::Dbo::ptr<Release>			_release;
		Wt::Dbo::collection< Wt::Dbo::ptr<Cluster> >	_clusters;
		Wt::Dbo::collection< Wt::Dbo::ptr<PlaylistEntry> >	_playlistEntries;

};

} // namespace database


