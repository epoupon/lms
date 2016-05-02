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

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

#include <Wt/WDateTime>

#include "SearchFilter.hpp"

namespace Database {


class Artist;
class Release;
class Track;
class PlaylistEntry;
class Feature;

class Cluster
{
	public:

		typedef Wt::Dbo::ptr<Cluster> pointer;
		typedef Wt::Dbo::dbo_traits<Cluster>::IdType id_type;

		Cluster();
		Cluster(std::string type, std::string name);

		// Find utility
		static pointer get(Wt::Dbo::Session& session, std::string type, std::string name);
		static pointer getNone(Wt::Dbo::Session& session);
		static std::vector<pointer> getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset = -1, int size = -1);
		static Wt::Dbo::collection<pointer> getAll(Wt::Dbo::Session& session);
		static Wt::Dbo::collection<pointer> getByType(Wt::Dbo::Session& session, std::stirng type);

		// MVC models for the user interface
		// ClusterID, type, name, track count
		typedef boost::tuple<id_type, std::string, std::string, int> UIQueryResult;
		static Wt::Dbo::Query<UIQueryResult> getUIQuery(Wt::Dbo::Session& session, SearchFilter filter);
		static void updateUIQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<UIQueryResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames = std::vector<Wt::WString>());

		// Create utility
		static pointer create(Wt::Dbo::Session& session, std::string type, std::string name);

		// Remove utility
		static void removeByType(Wt::Dbo::Session& session, std::string type);

		// Accessors
		const std::string& getName(void) const { return _name; }
		const std::string& getType(void) const { return _type; }
		bool isNone(void) const;
		const Wt::Dbo::collection< Wt::Dbo::ptr<Track> >&	getTracks() const { return _tracks;}

		void addTrack(Wt::Dbo::dbo_traits<Track>::IdType trackId);
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
		static std::vector<pointer> 	getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset = -1, int size = -1);
		static std::vector<pointer> 	getByFilter(Wt::Dbo::Session& session, SearchFilter filter, int offset, int size, bool &moreResults);
		static Wt::Dbo::collection< pointer > getAll(Wt::Dbo::Session& session);
		static std::vector<id_type> getAllIds(Wt::Dbo::Session& session);
		static std::vector<boost::filesystem::path> getAllPaths(Wt::Dbo::Session& session);
		static std::vector<pointer> getMBIDDuplicates(Wt::Dbo::Session& session);
		static std::vector<pointer> getChecksumDuplicates(Wt::Dbo::Session& session);

		// Utility fonctions
		// MVC models for the user interface
		// ID, Artist name, Release Name, DiscNumber, TrackNumber, Name, duration, date, original date, genre list
		typedef boost::tuple<id_type,			//ID
			std::string,				// Artist name
			std::string,				// Release Name
			int,					// Disc Number
			int,					// Track Number
			std::string,				// Name
			boost::posix_time::time_duration,	// Duration
			boost::posix_time::ptime,		// Date
			boost::posix_time::ptime,		// Original date
			std::string>				// genre list
			UIQueryResult;
		static Wt::Dbo::Query< UIQueryResult > getUIQuery(Wt::Dbo::Session& session, SearchFilter filter);
		static void updateUIQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel< UIQueryResult >& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames = std::vector<Wt::WString>());

		// Stats for a given search filter
		typedef boost::tuple<
				int,		// Total tracks
				boost::posix_time::time_duration // Total duration
				> StatsQueryResult;
		static StatsQueryResult getStats(Wt::Dbo::Session& session, SearchFilter filter);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, const boost::filesystem::path& p);

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
		void addFeature(Wt::Dbo::ptr<Feature> feature);

		int				getTrackNumber(void) const		{ return _trackNumber; }
		int				getTotalTrackNumber(void) const		{ return _totalTrackNumber; }
		int				getDiscNumber(void) const		{ return _discNumber; }
		int				getTotalDiscNumber(void) const		{ return _totalDiscNumber; }
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
		std::vector< Wt::Dbo::ptr<Feature> >	getFeatures(void) const;

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
				Wt::Dbo::hasMany(a, _features, Wt::Dbo::ManyToOne, "track");
			}

	private:

		static Wt::Dbo::Query< pointer > getQuery(Wt::Dbo::Session& session, SearchFilter filter);

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
		Wt::Dbo::collection< Wt::Dbo::ptr<Feature> > 	_features;
		Wt::Dbo::collection< Wt::Dbo::ptr<Cluster> >	_clusters;
		Wt::Dbo::collection< Wt::Dbo::ptr<PlaylistEntry> >	_playlistEntries;

};

/* A track feature */
class Feature
{
	public:
		typedef Wt::Dbo::ptr<Feature> pointer;

		Feature() {}
		Feature(Wt::Dbo::ptr<Track> track, const std::string& type, const std::string& value);

		static pointer create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Track> track, const std::string& type, const std::string& value);

		static std::vector<pointer> getByTrack(Wt::Dbo::Session& session, Track::id_type trackId, const std::string& type);

		std::string getType(void) const { return _type; }
		std::string getValue(void) const { return _value; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _type,	"type");
				Wt::Dbo::field(a, _value,	"value");
				Wt::Dbo::belongsTo(a,	_track, "track", Wt::Dbo::OnDeleteCascade);
			}

	private:
		std::string	_type;
		std::string	_value;

		Wt::Dbo::ptr<Track>	_track;
};

} // namespace database


