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

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Wt/WDateTime.h>
#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "utils/EnumSet.hpp"
#include "utils/UUID.hpp"

#include "services/database/ArtistId.hpp"
#include "services/database/ClusterId.hpp"
#include "services/database/Object.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/TrackListId.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"

namespace Database {

class Artist;
class Cluster;
class ClusterType;
class Release;
class Session;
class TrackArtistLink;
class TrackStats;
class User;

class Track final : public Object<Track, TrackId>
{
	public:
		struct FindParameters
		{
			std::vector<ClusterId>			clusters;		// if non empty, tracks that belong to these clusters
			std::vector<std::string_view>	keywords;		// if non empty, name must match all of these keywords
			std::string						name;			// if non empty, must match this name
			TrackSortMethod					sortMethod {TrackSortMethod::None};
			Range							range;
			Wt::WDateTime					writtenAfter;
			UserId							starringUser;	// only tracks starred by this user
			std::optional<Scrobbler>		scrobbler;		// and for this scrobbler
			ArtistId						artist;			// only tracks that involve this artist
			std::string						artistName;		// only tracks that involve this artist name
			EnumSet<TrackArtistLinkType>	trackArtistLinkTypes; 		//    and for these link types
			bool							nonRelease {};	// only tracks that do not belong to a release
			ReleaseId						release;		// matching this release
			std::string						releaseName;	// matching this release name
			TrackListId						trackList;		// matching this trackList
			std::optional<int>				trackNumber;	// matching this track number
			bool							distinct {true};

			FindParameters& setClusters(const std::vector<ClusterId>& _clusters) { clusters = _clusters; return *this; }
			FindParameters& setKeywords(const std::vector<std::string_view>& _keywords) { keywords = _keywords; return *this; }
			FindParameters& setName(std::string_view _name) { name = _name; return *this; }
			FindParameters& setSortMethod(TrackSortMethod _method) { sortMethod = _method; return *this; }
			FindParameters& setRange(Range _range) { range = _range; return *this; }
			FindParameters& setWrittenAfter(const Wt::WDateTime& _after) { writtenAfter = _after; return *this; }
			FindParameters& setStarringUser(UserId _user, Scrobbler _scrobbler) { starringUser = _user; scrobbler = _scrobbler; return *this; }
			FindParameters& setArtist(ArtistId _artist, EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {}) { artist = _artist; trackArtistLinkTypes = _trackArtistLinkTypes; return *this; }
			FindParameters& setArtistName(std::string_view _artistName, EnumSet<TrackArtistLinkType> _trackArtistLinkTypes = {}) { artistName = _artistName; trackArtistLinkTypes = _trackArtistLinkTypes; return *this; }
			FindParameters& setNonRelease(bool _nonRelease) { nonRelease = _nonRelease; return *this; }
			FindParameters& setRelease(ReleaseId _release) { release = _release; return *this; }
			FindParameters& setReleaseName(std::string_view _releaseName) { releaseName = _releaseName; return *this; }
			FindParameters& setTrackList(TrackListId _trackList) { trackList = _trackList; return *this; }
			FindParameters& setTrackNumber(int _trackNumber) { trackNumber = _trackNumber; return *this; }
			FindParameters& setDistinct(bool _distinct) { distinct = _distinct; return *this; }
		};

		struct PathResult
		{
			TrackId					trackId;
			std::filesystem::path	path;
		};

		Track() = default;

		// Find utility functions
		static std::size_t				getCount(Session& session);
		static pointer					findByPath(Session& session, const std::filesystem::path& p);
		static pointer 					find(Session& session, TrackId id);
		static bool						exists(Session& session, TrackId id);
		static std::vector<pointer>		findByRecordingMBID(Session& session, const UUID& MBID);
		static std::vector<pointer>		findByMBID(Session& session, const UUID& MBID);
		static RangeResults<TrackId>	findSimilarTracks(Session& session, const std::vector<TrackId>& trackIds, Range range);

		static RangeResults<TrackId>	find(Session& session, const FindParameters& parameters);
		static RangeResults<PathResult>	findPaths(Session& session, Range range);
		static RangeResults<TrackId>	findTrackMBIDDuplicates(Session& session, Range range);
		static RangeResults<TrackId>	findWithRecordingMBIDAndMissingFeatures(Session& session, Range range);

		// Accessors
		void setScanVersion(std::size_t version)				{ _scanVersion = version; }
		void setTrackNumber(std::optional<int> num)				{ _trackNumber = num; }
		void setDiscNumber(std::optional<int> num)				{ _discNumber = num; }
		void setTotalTrack(std::optional<int> totalTrack)		{ _totalTrack = totalTrack; }
		void setDiscSubtitle(const std::string& name)			{ _discSubtitle = name; }
		void setName(const std::string& name)					{ _name = std::string(name, 0, _maxNameLength); }
		void setPath(const std::filesystem::path& filePath) 	{ _filePath = filePath; }
		void setDuration(std::chrono::milliseconds duration)	{ _duration = duration; }
		void setLastWriteTime(Wt::WDateTime time)				{ _fileLastWrite = time; }
		void setAddedTime(Wt::WDateTime time)					{ _fileAdded = time; }
		void setDate(const Wt::WDate& date)						{ _date = date; }
		void setOriginalDate(const Wt::WDate& date)				{ _originalDate = date; }
		void setHasCover(bool hasCover)							{ _hasCover = hasCover; }
		void setTrackMBID(const std::optional<UUID>& MBID)		{ _trackMBID = MBID ? MBID->getAsString() : ""; }
		void setRecordingMBID(const std::optional<UUID>& MBID)	{ _recordingMBID = MBID ? MBID->getAsString() : ""; }
		void setCopyright(const std::string& copyright)			{ _copyright = std::string(copyright, 0, _maxCopyrightLength); }
		void setCopyrightURL(const std::string& copyrightURL)	{ _copyrightURL = std::string(copyrightURL, 0, _maxCopyrightURLLength); }
		void setTrackReplayGain(std::optional<float> replayGain)		{ _trackReplayGain = replayGain; }
		void setReleaseReplayGain(std::optional<float> replayGain)		{ _releaseReplayGain = replayGain; } // may be by disc!
		void setArtistDisplayName(std::string_view name)		{ _artistDisplayName = name; }
		void clearArtistLinks();
		void addArtistLink(const ObjectPtr<TrackArtistLink>& artistLink);
		void setRelease(ObjectPtr<Release> release)			{ _release = getDboPtr(release); }
		void setClusters(const std::vector<ObjectPtr<Cluster>>& clusters );

		std::size_t 				getScanVersion() const		{ return _scanVersion; }
		std::optional<std::size_t>	getTrackNumber() const		{ return _trackNumber; }
		std::optional<std::size_t>	getTotalTrack() const		{ return _totalTrack; }
		std::optional<std::size_t>	getDiscNumber() const		{ return _discNumber; }
		const std::string&			getDiscSubtitle() const { return _discSubtitle; }
		std::string 				getName() const			{ return _name; }
		std::filesystem::path		getPath() const			{ return _filePath; }
		std::chrono::milliseconds	getDuration() const		{ return _duration; }
		const Wt::WDateTime&		getLastWritten() const	{ return _fileLastWrite; }
		std::optional<int>			getYear() const;
		std::optional<int>			getOriginalYear() const;
		Wt::WDateTime				getLastWriteTime() const		{ return _fileLastWrite; }
		Wt::WDateTime				getAddedTime() const			{ return _fileAdded; }
		bool						hasCover() const				{ return _hasCover; }
		std::optional<UUID>			getTrackMBID() const			{ return UUID::fromString(_trackMBID); }
		std::optional<UUID>			getRecordingMBID() const		{ return UUID::fromString(_recordingMBID); }
		std::optional<std::string>	getCopyright() const;
		std::optional<std::string>	getCopyrightURL() const;
		std::optional<float>		getTrackReplayGain() const		{ return _trackReplayGain; }
		std::optional<float>		getReleaseReplayGain() const	{ return _releaseReplayGain; }
		std::string_view			getArtistDisplayName() const	{ return _artistDisplayName; }
		// no artistLinkTypes means get all
		std::vector<ObjectPtr<Artist>>			getArtists(EnumSet<TrackArtistLinkType> artistLinkTypes) const; // no type means all
		std::vector<ArtistId>					getArtistIds(EnumSet<TrackArtistLinkType> artistLinkTypes) const; // no type means all
		std::vector<ObjectPtr<TrackArtistLink>>	getArtistLinks() const;
		ObjectPtr<Release>						getRelease() const		{ return _release; }
		std::vector<ObjectPtr<Cluster>>			getClusters() const;
		std::vector<ClusterId>					getClusterIds() const;

		std::vector<std::vector<ObjectPtr<Cluster>>> getClusterGroups(const std::vector<ObjectPtr<ClusterType>>& clusterTypes, std::size_t size) const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _scanVersion,		"scan_version");
				Wt::Dbo::field(a, _trackNumber,		"track_number");
				Wt::Dbo::field(a, _discNumber,		"disc_number");
				Wt::Dbo::field(a, _totalTrack,		"total_track"); // here in Track since Release does not have concept of "disc" (yet?)
				Wt::Dbo::field(a, _discSubtitle,	"disc_subtitle"); // here in Track since Release does not have concept of "disc" (yet?)
				Wt::Dbo::field(a, _name,			"name");
				Wt::Dbo::field(a, _duration,		"duration");
				Wt::Dbo::field(a, _date,			"date");
				Wt::Dbo::field(a, _originalDate,	"original_date");
				Wt::Dbo::field(a, _filePath,		"file_path");
				Wt::Dbo::field(a, _fileLastWrite,	"file_last_write");
				Wt::Dbo::field(a, _fileAdded,		"file_added");
				Wt::Dbo::field(a, _hasCover,		"has_cover");
				Wt::Dbo::field(a, _trackMBID,		"mbid");
				Wt::Dbo::field(a, _recordingMBID,	"recording_mbid");
				Wt::Dbo::field(a, _copyright,		"copyright");
				Wt::Dbo::field(a, _copyrightURL,	"copyright_url");
				Wt::Dbo::field(a, _trackReplayGain,	"track_replay_gain");
				Wt::Dbo::field(a, _releaseReplayGain,	"release_replay_gain"); // here in Track since Release does not have concept of "disc" (yet?)
				Wt::Dbo::field(a, _artistDisplayName,	"artist_display_name");
				Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "track");
				Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
			}

	private:
		friend class ::Database::Session;
		Track(const std::filesystem::path& p);
		static pointer create(Session& session, const std::filesystem::path& p);

		static constexpr std::size_t _maxNameLength {128};
		static constexpr std::size_t _maxCopyrightLength {128};
		static constexpr std::size_t _maxCopyrightURLLength {128};

		int						_scanVersion {};
		std::optional<int>		_trackNumber {};
		std::optional<int>		_discNumber {};
		std::optional<int>		_totalTrack {};
		std::string				_discSubtitle;
		std::string				_name;
		std::chrono::duration<int, std::milli>	_duration {};
		Wt::WDate				_date;
		Wt::WDate				_originalDate;
		std::string				_filePath;
		Wt::WDateTime			_fileLastWrite;
		Wt::WDateTime			_fileAdded;
		bool					_hasCover {};
		std::string				_trackMBID;
		std::string				_recordingMBID;
		std::string				_copyright;
		std::string				_copyrightURL;
		std::optional<float>	_trackReplayGain;
		std::optional<float>	_releaseReplayGain;
		std::string				_artistDisplayName;

		Wt::Dbo::ptr<Release>								_release;
		Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>>	_trackArtistLinks;
		Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>>			_clusters;
};

namespace Debug
{
	struct TrackInfo
	{
		Session& session;
		TrackId trackId;
	};
	std::ostream& operator<<(std::ostream& os, const TrackInfo& trackInfo);
}

} // namespace database


