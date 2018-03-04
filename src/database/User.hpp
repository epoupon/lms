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

#ifndef DATABASE_USER_HPP
#define DATABASE_USER_HPP

#include <vector>

#include <Wt/Dbo/Dbo>
#include <Wt/Auth/Dbo/AuthInfo>

namespace Database {

class User;
typedef Wt::Auth::Dbo::AuthInfo<User> AuthInfo;

class Playlist;

// User selectable audio formats
enum class AudioEncoding
{
	AUTO,
	MP3,
	OGA,
	WEBMA,
};

enum class VideoEncoding
{
	AUTO,
};

class User
{

	public:

		static const std::size_t MaxNameLength = 15;

		// list of audio/video parameters
		static const std::vector<std::size_t> audioBitrates;
		static const std::vector<AudioEncoding> audioEncodings;

		static const std::vector<std::size_t> videoBitrates;
		static const std::vector<VideoEncoding> videoEncodings;

		User();

		typedef Wt::Dbo::ptr<User>	pointer;

		// utility
		static pointer create(Wt::Dbo::Session& session);

		// accessors
		static pointer			getById(Wt::Dbo::Session& session, id_type id);
		static std::vector<pointer>	getAll(Wt::Dbo::Session& session);
		static std::string		getId(pointer user);

		// write
		void setAdmin(bool admin)	{ _isAdmin = admin; }
		void setAudioBitrate(std::size_t bitrate);
		void setAudioEncoding(AudioEncoding encoding)	{ _audioEncoding = encoding; }
		void setVideoBitrate(std::size_t bitrate);
		void setVideoEncoding(VideoEncoding encoding)	{ _videoEncoding = encoding; }
		void setMaxAudioBitrate(std::size_t bitrate);
		void setMaxVideoBitrate(std::size_t bitrate);
		void setCurPlayingTrackPos(std::size_t pos) { _curPlayingTrackPos = pos; }

		// read
		bool isAdmin() const {return _isAdmin;}
		std::size_t	getAudioBitrate() const;
		AudioEncoding	getAudioEncoding() const { return _audioEncoding;}
		std::size_t	getVideoBitrate() const;
		VideoEncoding	getVideoEncoding() const { return _videoEncoding;}
		std::size_t	getMaxAudioBitrate() const;
		std::size_t	getMaxVideoBitrate() const;
		std::size_t	getCurPlayingTrackPos() const { return _curPlayingTrackPos; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _maxAudioBitrate, "max_audio_bitrate");
				Wt::Dbo::field(a, _maxVideoBitrate, "max_video_bitrate");
				Wt::Dbo::field(a, _isAdmin, "admin");
				Wt::Dbo::field(a, _audioBitrate, "audio_bitrate");
				Wt::Dbo::field(a, _audioEncoding, "audio_encoding");
				Wt::Dbo::field(a, _videoBitrate, "video_bitrate");
				Wt::Dbo::field(a, _videoEncoding, "video_encoding");
				// User's dynamic data
				Wt::Dbo::field(a, _curPlayingTrackPos, "cur_playing_track_pos");
				Wt::Dbo::hasMany(a, _playlists, Wt::Dbo::ManyToOne, "user");
			}

	private:

		static const std::size_t	maxAudioBitrate = 320000;
		static const std::size_t	maxVideoBitrate = 2048000;

		static const std::size_t	defaultAudioBitrate = 128000;
		static const std::size_t	defaultVideoBitrate = 1024000;

		// Admin defined settings
		int	 	_maxAudioBitrate;
		int		_maxVideoBitrate;
		bool		_isAdmin;

		// User defined settings
		int		_audioBitrate;
		AudioEncoding	_audioEncoding;
		int		_videoBitrate;
		VideoEncoding	_videoEncoding;

		// User's dynamic data
		int		_curPlayingTrackPos;	// Current track position in queue

		Wt::Dbo::collection< Wt::Dbo::ptr<Playlist> > _playlists;

};

} // namespace Databas'

#endif
