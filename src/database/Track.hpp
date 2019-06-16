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
#include <chrono>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "TrackArtistLink.hpp"
#include "Types.hpp"

namespace Database {

class Artist;
class Cluster;
class ClusterType;
class Release;
class TrackFeatures;
class TrackListEntry;
class TrackStats;
class User;

class Track : public Wt::Dbo::Dbo<Track>
{
	public:

		using pointer = Wt::Dbo::ptr<Track>;

		Track() {}
		Track(const boost::filesystem::path& p);

		// Find utility functions
		static pointer getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p);
		static pointer getById(Wt::Dbo::Session& session, IdType id);
		static pointer getByMBID(Wt::Dbo::Session& session, const std::string& MBID);
		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session,
							const std::set<IdType>& clusters);           // tracks that belong to these clusters
		static std::vector<pointer>	getByFilter(Wt::Dbo::Session& session,
							const std::set<IdType>& clusters,           // tracks that belong to these clusters
							const std::vector<std::string> keywords,        // name must match all of these keywords
							boost::optional<std::size_t> offset,
							boost::optional<std::size_t> size,
							bool& moreExpected);

		static std::vector<pointer>	getAll(Wt::Dbo::Session& session, boost::optional<std::size_t> limit = {});
		static std::vector<pointer>	getAllRandom(Wt::Dbo::Session& session, boost::optional<std::size_t> limit = {});
		static std::vector<IdType>	getAllIds(Wt::Dbo::Session& session); // nested transaction
		static std::vector<boost::filesystem::path> getAllPaths(Wt::Dbo::Session& session); // nested transaction
		static std::vector<pointer>	getMBIDDuplicates(Wt::Dbo::Session& session);
		static std::vector<pointer>	getChecksumDuplicates(Wt::Dbo::Session& session);
		static std::vector<pointer>	getLastAdded(Wt::Dbo::Session& session, Wt::WDateTime after, int size = 1);
		static std::vector<pointer>	getAllWithMBIDAndMissingFeatures(Wt::Dbo::Session& session); // nested transaction
		static std::vector<IdType>	getAllIdsWithFeatures(Wt::Dbo::Session& session, boost::optional<std::size_t> limit = {}); // nested transaction

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, const boost::filesystem::path& p);

		// Accessors
		void setScanVersion(std::size_t version)			{ _scanVersion = version; }
		void setTrackNumber(int num)					{ _trackNumber = num; }
		void setDiscNumber(int num)					{ _discNumber = num; }
		void setName(const std::string& name)				{ _name = std::string(name, 0, _maxNameLength); }
		void setDuration(std::chrono::milliseconds duration)		{ _duration = duration; }
		void setLastWriteTime(Wt::WDateTime time)			{ _fileLastWrite = time; }
		void setAddedTime(Wt::WDateTime time)				{ _fileAdded = time; }
		void setChecksum(const std::vector<unsigned char>& checksum)	{ _fileChecksum = checksum; }
		void setYear(int year)						{ _year = year; }
		void setOriginalYear(int year)					{ _originalYear = year; }
		void setHasCover(bool hasCover)					{ _hasCover = hasCover; }
		void setMBID(const std::string& MBID)				{ _MBID = MBID; }
		void setCopyright(const std::string& copyright)			{ _copyright = std::string(copyright, 0, _maxCopyrightLength); }
		void setCopyrightURL(const std::string& copyrightURL)		{ _copyrightURL = std::string(copyrightURL, 0, _maxCopyrightURLLength); }
		void clearArtistLinks();
		void addArtistLink(const Wt::Dbo::ptr<TrackArtistLink>& artistLink);
		void setRelease(Wt::Dbo::ptr<Release> release)			{ _release = release; }
		void setClusters(const std::vector<Wt::Dbo::ptr<Cluster>>& clusters );
		void setFeatures(const Wt::Dbo::ptr<TrackFeatures>& features);

		std::size_t 				getScanVersion() const		{ return _scanVersion; }
		boost::optional<std::size_t>		getTrackNumber() const;
		boost::optional<std::size_t>		getDiscNumber() const;
		std::string 				getName() const			{ return _name; }
		boost::filesystem::path			getPath() const			{ return _filePath; }
		std::chrono::milliseconds		getDuration() const		{ return _duration; }
		boost::optional<int>			getYear() const;
		boost::optional<int>			getOriginalYear() const;
		Wt::WDateTime				getLastWriteTime() const	{ return _fileLastWrite; }
		Wt::WDateTime				getAddedTime() const		{ return _fileAdded; }
		const std::vector<unsigned char>&	getChecksum() const		{ return _fileChecksum; }
		bool					hasCover() const		{ return _hasCover; }
		const std::string&			getMBID() const			{ return _MBID; }
		boost::optional<std::string>		getCopyright() const;
		boost::optional<std::string>		getCopyrightURL() const;
		std::vector<Wt::Dbo::ptr<Artist>>	getArtists(TrackArtistLink::Type type = {TrackArtistLink::Type::Artist}) const;
		std::vector<Wt::Dbo::ptr<TrackArtistLink>>	getArtistLinks() const;
		Wt::Dbo::ptr<Release>			getRelease() const		{ return _release; }
		std::vector<Wt::Dbo::ptr<Cluster>>	getClusters() const;
		bool					hasTrackFeatures() const;
		Wt::Dbo::ptr<TrackFeatures>		getTrackFeatures() const;

		std::vector<std::vector<Wt::Dbo::ptr<Cluster>>> getClusterGroups(std::vector<Wt::Dbo::ptr<ClusterType>> clusterTypes, std::size_t size) const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _scanVersion,		"scan_version");
				Wt::Dbo::field(a, _trackNumber,		"track_number");
				Wt::Dbo::field(a, _discNumber,		"disc_number");
				Wt::Dbo::field(a, _name,		"name");
				Wt::Dbo::field(a, _duration,		"duration");
				Wt::Dbo::field(a, _year,		"year");
				Wt::Dbo::field(a, _originalYear,	"original_year");
				Wt::Dbo::field(a, _filePath,		"file_path");
				Wt::Dbo::field(a, _fileLastWrite,	"file_last_write");
				Wt::Dbo::field(a, _fileAdded,		"file_added");
				Wt::Dbo::field(a, _fileChecksum,	"checksum");
				Wt::Dbo::field(a, _hasCover,		"has_cover");
				Wt::Dbo::field(a, _MBID,		"mbid");
				Wt::Dbo::field(a, _copyright,		"copyright");
				Wt::Dbo::field(a, _copyrightURL,	"copyright_url");
				Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "track");
				Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _playlistEntries, Wt::Dbo::ManyToOne, "track");
				Wt::Dbo::hasMany(a, _starringUsers, Wt::Dbo::ManyToMany, "user_track_starred", "", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasOne(a, _trackFeatures);
			}

	private:

		static const std::size_t _maxNameLength = 128;
		static const std::size_t _maxCopyrightLength = 128;
		static const std::size_t _maxCopyrightURLLength = 128;

		int					_scanVersion = 0;
		int					_trackNumber = 0;
		int					_discNumber = 0;
		std::string				_name;
		std::string				_artistName;
		std::string				_releaseName;
		std::chrono::duration<int, std::milli>	_duration;
		int					_year = 0;
		int					_originalYear = 0;
		std::string				_filePath;
		std::vector<unsigned char>		_fileChecksum;
		Wt::WDateTime				_fileLastWrite;
		Wt::WDateTime				_fileAdded;
		bool					_hasCover = false;
		std::string				_MBID; // Musicbrainz Identifier
		std::string				_copyright;
		std::string				_copyrightURL;

		Wt::Dbo::ptr<Release>				_release;
		Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>> _trackArtistLinks;
		Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> 	_clusters;
		Wt::Dbo::collection<Wt::Dbo::ptr<TrackListEntry>> _playlistEntries;
		Wt::Dbo::collection<Wt::Dbo::ptr<User>>		_starringUsers;
		Wt::Dbo::weak_ptr<TrackFeatures>		_trackFeatures;

};

} // namespace database


