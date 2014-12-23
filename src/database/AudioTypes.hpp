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

#ifndef _AUDIO_TYPES_HPP_
#define _AUDIO_TYPES_HPP_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

#include <Wt/WDateTime>

namespace Database {

// Find utilities
struct SearchFilter
{
	enum class Field {
		Artist,		// artist name
		Release,	// release name
		Genre,		// genre name
		Track,		// track name
	};

	typedef std::map<Field, std::vector<std::string> > FieldValues;

	// Formulas :
	// ((like1-1 LIKE ? OR like1-2 LIKE = ? ...) AND (like2-1 LIKE ? OR like2-2 LIKE ? ...)) AND exact1 = ? AND exact2 = ? ...

	std::vector<FieldValues> likeMatches;
	FieldValues exactMatch;
};

class Track;

class Genre
{
	public:

		typedef Wt::Dbo::ptr<Genre> pointer;
		typedef Wt::Dbo::dbo_traits<Genre>::IdType id_type;

		Genre();
		Genre(const std::string& name);

		// Find utility
		static pointer getByName(Wt::Dbo::Session& session, const std::string& name);
		static pointer getNone(Wt::Dbo::Session& session);
		static Wt::Dbo::collection<pointer> getAll(Wt::Dbo::Session& session, int offset = -1, int size = -1);
		typedef boost::tuple<std::string, int> GenreResult;
		static Wt::Dbo::Query<GenreResult> getAllQuery(Wt::Dbo::Session& session, SearchFilter& filter);
		static void updateGenreQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<GenreResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames = std::vector<Wt::WString>());

		// Create utility
		static pointer create(Wt::Dbo::Session& session, const std::string& name);

		// Accessors
		const std::string& getName(void) const { return _name; }
		bool isNone(void) const;
		const Wt::Dbo::collection< Wt::Dbo::ptr<Track> >&	getTracks() const { return _tracks;}

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name,	"name");
				Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_genre", "", Wt::Dbo::OnDeleteCascade);
			}

	private:
		static const std::size_t _maxNameLength = 128;
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


		// Find utility functions
		static pointer getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p);
		static pointer getById(Wt::Dbo::Session& session, id_type id);
		static Wt::Dbo::collection< pointer > getAll(Wt::Dbo::Session& session);
		// Used for remote
		static Wt::Dbo::collection< pointer > getAll(Wt::Dbo::Session& session, SearchFilter filter, int offset = -1, int size = -1);
		static std::vector<std::string> getReleases(Wt::Dbo::Session& session, SearchFilter filter, int offset = -1, int size = -1);
		static std::vector<std::string> getArtists(Wt::Dbo::Session& session, SearchFilter filter, int offset = -1, int size = -1);

		// Utility fonctions
		// MVC models for the user interface
		static void updateTracksQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel< pointer >& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames = std::vector<Wt::WString>());
		// Release name, year, track counts
		typedef boost::tuple<std::string, boost::posix_time::ptime, int> ReleaseResult;
		static void updateReleaseQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<ReleaseResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames = std::vector<Wt::WString>());
		// Artist name, albums, tracks
		typedef boost::tuple<std::string, int, int> ArtistResult;
		static void updateArtistQueryModel(Wt::Dbo::Session& session, Wt::Dbo::QueryModel<ArtistResult>& model, SearchFilter filter, const std::vector<Wt::WString>& columnNames = std::vector<Wt::WString>());

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, const boost::filesystem::path& p);

		// Accessors
		void setTrackNumber(int num)					{ _trackNumber = num; }
		void setDiscNumber(int num)					{ _discNumber = num; }
		void setName(const std::string& name)				{ _name = std::string(name, 0, _maxNameLength); }
		void setArtistName(const std::string& name)				{ _artistName = std::string(name, 0, _maxNameLength); }
		void setReleaseName(const std::string& name)				{ _releaseName = std::string(name, 0, _maxNameLength); }
		void setDuration(boost::posix_time::time_duration duration)	{ _duration = duration; }
		void setLastWriteTime(boost::posix_time::ptime time)		{ _fileLastWrite = time; }
		void setChecksum(const std::vector<unsigned char>& checksum)	{ _fileChecksum = checksum; }
		void setDate(const boost::posix_time::ptime& date)		{ _date = date; }
		void setOriginalDate(const boost::posix_time::ptime& date)	{ _originalDate = date; }
		void setGenres(const std::string& genreList)			{ _genreList = genreList; }
		void setGenres(std::vector<Genre::pointer> genres);
		void setHasCover(bool hasCover)					{ _hasCover = hasCover; }

		int				getTrackNumber(void) const		{ return _trackNumber; }
		int				getDiscNumber(void) const		{ return _discNumber; }
		std::string 			getName(void) const			{ return _name; }
		std::string 			getArtistName(void) const			{ return _artistName; }
		std::string 			getReleaseName(void) const			{ return _releaseName; }
		const std::string&		getPath(void) const			{ return _filePath; }
		boost::posix_time::time_duration	getDuration(void) const		{ return _duration; }
		boost::posix_time::ptime	getDate(void) const			{ return _date; }
		boost::posix_time::ptime	getOriginalDate(void) const		{ return _originalDate; }
		bool				hasGenre(Genre::pointer genre) const	{ return _genres.count(genre); }
		std::vector< Genre::pointer >	getGenres(void) const;

		boost::posix_time::ptime	getLastWriteTime(void) const		{ return _fileLastWrite; }
		const std::vector<unsigned char>& getChecksum(void) const		{ return _fileChecksum; }
		bool				hasCover(void) const			{ return _hasCover; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _trackNumber,		"track_number");
				Wt::Dbo::field(a, _discNumber,		"disc_number");
				Wt::Dbo::field(a, _name,		"name");
				Wt::Dbo::field(a, _artistName,		"artist_name");
				Wt::Dbo::field(a, _releaseName,		"release_name");
				Wt::Dbo::field(a, _duration,		"duration");
				Wt::Dbo::field(a, _date,		"date");
				Wt::Dbo::field(a, _originalDate,	"original_date");
				Wt::Dbo::field(a, _genreList,		"genre_list");
				Wt::Dbo::field(a, _filePath,		"path");
				Wt::Dbo::field(a, _fileLastWrite,	"last_write");
				Wt::Dbo::field(a, _fileChecksum,	"checksum");
				Wt::Dbo::field(a, _hasCover,		"has_cover");
				Wt::Dbo::hasMany(a, _genres, Wt::Dbo::ManyToMany, "track_genre", "", Wt::Dbo::OnDeleteCascade);
			}

	private:

		static Wt::Dbo::Query< pointer > getAllQuery(Wt::Dbo::Session& session, SearchFilter filter);
		static Wt::Dbo::Query<ReleaseResult> getReleasesQuery(Wt::Dbo::Session& session, SearchFilter filter);
		static Wt::Dbo::Query<ArtistResult> getArtistsQuery(Wt::Dbo::Session& session, SearchFilter filter);

		static const std::size_t _maxNameLength = 128;

		int					_trackNumber;
		int					_discNumber;
		std::string				_name;
		std::string				_artistName;
		std::string				_releaseName;
		boost::posix_time::time_duration	_duration;
		boost::posix_time::ptime		_date;
		boost::posix_time::ptime		_originalDate;
		std::string				_genreList;
		std::string				_filePath;
		std::vector<unsigned char>		_fileChecksum;
		boost::posix_time::ptime		_fileLastWrite;
		bool					_hasCover;

		Wt::Dbo::collection< Genre::pointer > _genres;	// Tracks that belong to this genre
};


} // namespace database

#endif

