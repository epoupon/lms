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

#include <optional>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Dbo.h>
#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "utils/UUID.hpp"

namespace Database {


class Artist;
class Release;
class Session;
class TrackList;
class Track;

class User;
class AuthToken : public Object<AuthToken, AuthTokenId>
{
	public:
		AuthToken() = default;
		AuthToken(const std::string& value, const Wt::WDateTime& expiry, ObjectPtr<User> user);

		// Utility
		static pointer create(Session& session, const std::string& value, const Wt::WDateTime&expiry, ObjectPtr<User> user);
		static void removeExpiredTokens(Session& session, const Wt::WDateTime& now);
		static pointer getByValue(Session& session, const std::string& value);
		static pointer getById(Session& session, AuthTokenId tokenId);

		// Accessors
		const Wt::WDateTime&	getExpiry() const { return _expiry; }
		ObjectPtr<User>	getUser() const { return _user; }
		const std::string&	getValue() const { return _value; }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,	_value,		"value");
			Wt::Dbo::field(a,	_expiry,	"expiry");
			Wt::Dbo::belongsTo(a,	_user,		"user", Wt::Dbo::OnDeleteCascade);
		}

	private:

		std::string		_value;
		Wt::WDateTime		_expiry;

		Wt::Dbo::ptr<User>	_user;
};

class User : public Object<User, UserId>
{
	public:
		struct PasswordHash
		{
			std::string salt;
			std::string hash;
		};

		// Do not change enum values!
		enum class UITheme
		{
			Light	= 0,
			Dark	= 1,
		};

		// Do not remove values!
		static inline const std::set<Bitrate>	audioTranscodeAllowedBitrates
		{
			64000,
			96000,
			128000,
			192000,
			320000,
		};

		// Do not change enum values!
		enum class SubsonicArtistListMode
		{
			AllArtists		= 0,
			ReleaseArtists	= 1,
			TrackArtists	= 2,
		};

		static inline const std::size_t 	MinNameLength {3};
		static inline const std::size_t 	MaxNameLength {15};
		static inline const bool		defaultSubsonicTranscodeEnable {true};
		static inline const AudioFormat		defaultSubsonicTranscodeFormat {AudioFormat::OGG_OPUS};
		static inline const Bitrate		defaultSubsonicTranscodeBitrate {128000};
		static inline const UITheme		defaultUITheme {UITheme::Dark};
		static inline const SubsonicArtistListMode defaultSubsonicArtistListMode {SubsonicArtistListMode::AllArtists};
		static inline const Scrobbler	defaultScrobbler {Scrobbler::Internal};

		User() = default;
		User(std::string_view loginName);

		// utility
		static pointer create(Session& session, std::string_view loginName);

		static pointer			getById(Session& session, UserId id);
		static pointer			getByLoginName(Session& session, std::string_view loginName);
		static std::vector<pointer>	getAll(Session& session);
		static std::vector<UserId>	getAllIds(Session& session);
		static pointer			getDemo(Session& session);
		static std::size_t		getCount(Session& session);

		// accessors
		const std::string& getLoginName() const { return _loginName; }
		PasswordHash getPasswordHash() const { return PasswordHash {_passwordSalt, _passwordHash}; }
		Wt::WDateTime getLastLogin() const { return _lastLogin; }
		std::size_t getAuthTokensCount() const { return _authTokens.size(); }

		// write
		void setLastLogin(const Wt::WDateTime& dateTime)	{ _lastLogin = dateTime; }
		void setPasswordHash(const PasswordHash& passwordHash)	{ _passwordSalt = passwordHash.salt; _passwordHash = passwordHash.hash; }
		void setType(UserType type)					{ _type = type; }
		void setSubsonicTranscodeEnable(bool value) 		{ _subsonicTranscodeEnable = value; }
		void setSubsonicTranscodeFormat(AudioFormat encoding)	{ _subsonicTranscodeFormat = encoding; }
		void setSubsonicTranscodeBitrate(Bitrate bitrate);
		void setCurPlayingTrackPos(std::size_t pos)		{ _curPlayingTrackPos = pos; }
		void setRadio(bool val)					{ _radio = val; }
		void setRepeatAll(bool val)				{ _repeatAll = val; }
		void setUITheme(UITheme uiTheme)			{ _uiTheme = uiTheme; }
		void clearAuthTokens();
		void setSubsonicArtistListMode(SubsonicArtistListMode mode)	{ _subsonicArtistListMode = mode; }
		void setScrobbler(Scrobbler scrobbler)	{ _scrobbler = scrobbler; }
		void setListenBrainzToken(const std::optional<UUID>& MBID)	{ _listenbrainzToken = MBID ? MBID->getAsString() : ""; }

		// read
		bool			isAdmin() const { return _type == UserType::ADMIN; }
		bool			isDemo() const { return _type == UserType::DEMO; }
		UserType		getType() const { return _type; }
		bool			getSubsonicTranscodeEnable() const { return _subsonicTranscodeEnable; }
		AudioFormat		getSubsonicTranscodeFormat() const { return _subsonicTranscodeFormat; }
		Bitrate			getSubsonicTranscodeBitrate() const { return _subsonicTranscodeBitrate; }
		std::size_t		getCurPlayingTrackPos() const { return _curPlayingTrackPos; }
		bool			isRepeatAllSet() const { return _repeatAll; }
		bool			isRadioSet() const { return _radio; }
		UITheme			getUITheme() const { return _uiTheme; }
		SubsonicArtistListMode	getSubsonicArtistListMode() const { return _subsonicArtistListMode; }
		Scrobbler				getScrobbler() const { return _scrobbler; }
		std::optional<UUID>		getListenBrainzToken() const	{ return UUID::fromString(_listenbrainzToken); }

		ObjectPtr<TrackList>	getQueuedTrackList(Session& session) const;

		void			starArtist(ObjectPtr<Artist> artist);
		void			unstarArtist(ObjectPtr<Artist> artist);
		bool			hasStarredArtist(ObjectPtr<Artist> artist) const;

		void			starRelease(ObjectPtr<Release> release);
		void			unstarRelease(ObjectPtr<Release> release);
		bool			hasStarredRelease(ObjectPtr<Release> release) const;

		// Stars
		void			starTrack(ObjectPtr<Track> track);
		void			unstarTrack(ObjectPtr<Track> track);
		bool			hasStarredTrack(ObjectPtr<Track> track) const;

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _type, "type");
			Wt::Dbo::field(a, _loginName, "login_name");
			Wt::Dbo::field(a, _passwordSalt, "password_salt");
			Wt::Dbo::field(a, _passwordHash, "password_hash");
			Wt::Dbo::field(a, _lastLogin, "last_login");
			Wt::Dbo::field(a, _subsonicTranscodeEnable, "subsonic_transcode_enable");
			Wt::Dbo::field(a, _subsonicTranscodeFormat, "subsonic_transcode_format");
			Wt::Dbo::field(a, _subsonicTranscodeBitrate, "subsonic_transcode_bitrate");
			Wt::Dbo::field(a, _subsonicArtistListMode, "subsonic_artist_list_mode");
			Wt::Dbo::field(a, _uiTheme, "ui_theme");
			Wt::Dbo::field(a, _scrobbler, "scrobbler");
			Wt::Dbo::field(a, _listenbrainzToken, "listenbrainz_token");

			// UI settings
			Wt::Dbo::field(a, _curPlayingTrackPos, "cur_playing_track_pos");
			Wt::Dbo::field(a, _repeatAll, "repeat_all");
			Wt::Dbo::field(a, _radio, "radio");

			Wt::Dbo::hasMany(a, _tracklists, Wt::Dbo::ManyToOne, "user");
			Wt::Dbo::hasMany(a, _starredArtists, Wt::Dbo::ManyToMany, "user_artist_starred", "", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _starredReleases, Wt::Dbo::ManyToMany, "user_release_starred", "", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _starredTracks, Wt::Dbo::ManyToMany, "user_track_starred", "", Wt::Dbo::OnDeleteCascade);
			Wt::Dbo::hasMany(a, _authTokens, Wt::Dbo::ManyToOne, "user");
		}

	private:

		std::string	_loginName;
		std::string	_passwordSalt;
		std::string	_passwordHash;
		Wt::WDateTime	_lastLogin;
		UITheme		_uiTheme {defaultUITheme};
		Scrobbler	_scrobbler {defaultScrobbler};
		std::string	_listenbrainzToken; // Musicbrainz Identifier

		// Admin defined settings
		UserType		_type {UserType::REGULAR};

		// User defined settings
		SubsonicArtistListMode	_subsonicArtistListMode {defaultSubsonicArtistListMode};
		bool			_subsonicTranscodeEnable {defaultSubsonicTranscodeEnable};
		AudioFormat		_subsonicTranscodeFormat {defaultSubsonicTranscodeFormat};
		int				_subsonicTranscodeBitrate {defaultSubsonicTranscodeBitrate};

		// User's dynamic data (UI)
		int		_curPlayingTrackPos {}; // Current track position in queue
		bool		_repeatAll {};
		bool		_radio {};

		Wt::Dbo::collection<Wt::Dbo::ptr<TrackList>> _tracklists;
		Wt::Dbo::collection<Wt::Dbo::ptr<Artist>> _starredArtists;
		Wt::Dbo::collection<Wt::Dbo::ptr<Release>> _starredReleases;
		Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _starredTracks;
		Wt::Dbo::collection<Wt::Dbo::ptr<AuthToken>> _authTokens;

};

} // namespace Databas'

