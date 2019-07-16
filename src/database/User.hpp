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

#pragma once

#include <vector>

#include <Wt/Dbo/Dbo.h>

#include "Types.hpp"

namespace Database {


class Artist;
class Release;
class Session;
class TrackList;
class Track;

// User selectable audio formats
// Do not change values
enum class AudioFormat
{
	MP3		= 1,
	OGG_OPUS	= 2,
	OGG_VORBIS	= 3,
	WEBM_VORBIS	= 4,
};

using Bitrate = std::size_t;

class User : public Wt::Dbo::Dbo<User>
{
	public:
		using pointer = Wt::Dbo::ptr<User>;

		static const std::size_t MinNameLength = 3;
		static const std::size_t MaxNameLength = 15;

		enum class Type
		{
			REGULAR,
			ADMIN,
			DEMO
		};

		// list of audio parameters
		static const std::set<Bitrate> audioTranscodeAllowedBitrates;

		User();

		// utility
		static pointer create(Session& session);

		// accessors
		static pointer			getById(Session& session, IdType id);
		static pointer			getByLoginName(const std::string& loginName);
		static std::vector<pointer>	getAll(Session& session);
		static pointer			getDemo(Session& session);

		// write
		void setType(Type type)					{ _type = type; }
		void setAudioTranscodeEnable(bool value) 		{ _audioTranscodeEnable = value; }
		void setAudioTranscodeFormat(AudioFormat format)	{ _audioTranscodeFormat = format; }
		void setAudioTranscodeBitrate(Bitrate bitrate);
		void setMaxAudioTranscodeBitrate(Bitrate bitrate);
		void setCurPlayingTrackPos(std::size_t pos)		{ _curPlayingTrackPos = pos; }
		void setRadio(bool val)					{ _radio = val; }
		void setRepeatAll(bool val)				{ _repeatAll = val; }

		// read
		bool			isAdmin() const { return _type == Type::ADMIN; }
		bool			isDemo() const { return _type == Type::DEMO; }
		bool			getAudioTranscodeEnable() const { return _audioTranscodeEnable; }
		Bitrate			getAudioTranscodeBitrate() const;
		AudioFormat		getAudioTranscodeFormat() const { return _audioTranscodeFormat; }
		Bitrate			getMaxAudioTranscodeBitrate() const;
		std::size_t		getCurPlayingTrackPos() const { return _curPlayingTrackPos; }
		bool			isRepeatAllSet() const { return _repeatAll; }
		bool			isRadioSet() const { return _radio; }

		Wt::Dbo::ptr<TrackList>	getPlayedTrackList(Session& session) const;
		Wt::Dbo::ptr<TrackList>	getQueuedTrackList(Session& session) const;

		void			starArtist(Wt::Dbo::ptr<Artist> artist);
		void			unstarArtist(Wt::Dbo::ptr<Artist> artist);
		bool			hasStarredArtist(Wt::Dbo::ptr<Artist> artist) const;
		std::vector<Wt::Dbo::ptr<Artist>> getStarredArtists() const;

		void			starRelease(Wt::Dbo::ptr<Release> release);
		void			unstarRelease(Wt::Dbo::ptr<Release> release);
		bool			hasStarredRelease(Wt::Dbo::ptr<Release> release) const;
		std::vector<Wt::Dbo::ptr<Release>> getStarredReleases() const;

		void			starTrack(Wt::Dbo::ptr<Track> track);
		void			unstarTrack(Wt::Dbo::ptr<Track> track);
		bool			hasStarredTrack(Wt::Dbo::ptr<Track> track) const;
		std::vector<Wt::Dbo::ptr<Track>> getStarredTracks() const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _type, "type");
				Wt::Dbo::field(a, _maxAudioTranscodeBitrate, "max_audio_bitrate");
				Wt::Dbo::field(a, _audioTranscodeEnable, "audio_transcode_enable");
				Wt::Dbo::field(a, _audioTranscodeBitrate, "audio_transcode_bitrate");
				Wt::Dbo::field(a, _audioTranscodeFormat, "audio_transcode_format");
				// User's dynamic data
				Wt::Dbo::field(a, _curPlayingTrackPos, "cur_playing_track_pos");
				Wt::Dbo::field(a, _repeatAll, "repeat_all");
				Wt::Dbo::field(a, _radio, "radio");
				Wt::Dbo::hasMany(a, _tracklists, Wt::Dbo::ManyToOne, "user");
				Wt::Dbo::hasMany(a, _starredArtists, Wt::Dbo::ManyToMany, "user_artist_starred", "", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _starredReleases, Wt::Dbo::ManyToMany, "user_release_starred", "", Wt::Dbo::OnDeleteCascade);
				Wt::Dbo::hasMany(a, _starredTracks, Wt::Dbo::ManyToMany, "user_track_starred", "", Wt::Dbo::OnDeleteCascade);
			}

	private:

		static const bool		defaultAudioTranscodeEnable {true};
		static const AudioFormat	defaultAudioTranscodeFormat {AudioFormat::OGG_OPUS};
		static const Bitrate		defaultAudioTranscodeBitrate {128000};

		// Admin defined settings
		int	 	_maxAudioTranscodeBitrate;
		Type		_type {Type::REGULAR};

		// User defined settings
		bool			_audioTranscodeEnable {defaultAudioTranscodeEnable};
		AudioFormat		_audioTranscodeFormat {defaultAudioTranscodeFormat};
		int			_audioTranscodeBitrate {defaultAudioTranscodeBitrate};

		// User's dynamic data
		int		_curPlayingTrackPos {}; // Current track position in queue
		bool		_repeatAll {};
		bool		_radio {};

		Wt::Dbo::collection<Wt::Dbo::ptr<TrackList>> _tracklists;
		Wt::Dbo::collection<Wt::Dbo::ptr<Artist>> _starredArtists;
		Wt::Dbo::collection<Wt::Dbo::ptr<Release>> _starredReleases;
		Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _starredTracks;

};

} // namespace Databas'

