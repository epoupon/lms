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

#include "services/database/Object.hpp"
#include "services/database/Types.hpp"
#include "services/database/UserId.hpp"
#include "utils/UUID.hpp"

namespace Database {

    class AuthToken;
    class Session;

    class User final : public Object<User, UserId>
    {
    public:
        struct PasswordHash
        {
            std::string salt;
            std::string hash;
        };

        struct FindParameters
        {
            std::optional<Scrobbler>		scrobbler;
            Range							range;

            FindParameters& setScrobbler(Scrobbler _scrobbler) { scrobbler = _scrobbler; return *this; }
            FindParameters& setRange(Range _range) { range = _range; return *this; }
        };

        static inline const std::size_t             MinNameLength{ 3 };
        static inline const std::size_t             MaxNameLength{ 15 };
        static inline const AudioFormat             defaultSubsonicTranscodeFormat{ AudioFormat::OGG_OPUS };
        static inline const Bitrate                 defaultSubsonicTranscodeBitrate{ 128000 };
        static inline const UITheme                 defaultUITheme{ UITheme::Dark };
        static inline const SubsonicArtistListMode  defaultSubsonicArtistListMode{ SubsonicArtistListMode::AllArtists };
        static inline const Scrobbler               defaultScrobbler{ Scrobbler::Internal };

        User() = default;

        static std::size_t          getCount(Session& session);
        static pointer              find(Session& session, UserId id);
        static pointer              find(Session& session, std::string_view loginName);
        static RangeResults<UserId> find(Session& session, const FindParameters& params);
        static pointer              findDemoUser(Session& session);

        // accessors
        const std::string& getLoginName() const { return _loginName; }
        PasswordHash            getPasswordHash() const { return PasswordHash{ _passwordSalt, _passwordHash }; }
        const Wt::WDateTime& getLastLogin() const { return _lastLogin; }
        std::size_t             getAuthTokensCount() const { return _authTokens.size(); }

        // write
        void setLastLogin(const Wt::WDateTime& dateTime) { _lastLogin = dateTime; }
        void setPasswordHash(const PasswordHash& passwordHash) { _passwordSalt = passwordHash.salt; _passwordHash = passwordHash.hash; }
        void setType(UserType type) { _type = type; }
        void setSubsonicDefaultTranscodeFormat(AudioFormat encoding) { _subsonicDefaultTranscodeFormat = encoding; }
        void setSubsonicDefaultTranscodeBitrate(Bitrate bitrate);
        void setCurPlayingTrackPos(std::size_t pos) { _curPlayingTrackPos = pos; }
        void setRadio(bool val) { _radio = val; }
        void setRepeatAll(bool val) { _repeatAll = val; }
        void setUITheme(UITheme uiTheme) { _uiTheme = uiTheme; }
        void clearAuthTokens();
        void setSubsonicArtistListMode(SubsonicArtistListMode mode) { _subsonicArtistListMode = mode; }
        void setScrobbler(Scrobbler scrobbler) { _scrobbler = scrobbler; }
        void setListenBrainzToken(const std::optional<UUID>& MBID) { _listenbrainzToken = MBID ? MBID->getAsString() : ""; }

        // read
        bool                    isAdmin() const { return _type == UserType::ADMIN; }
        bool                    isDemo() const { return _type == UserType::DEMO; }
        UserType                getType() const { return _type; }
        AudioFormat             getSubsonicDefaultTranscodeFormat() const { return _subsonicDefaultTranscodeFormat; }
        Bitrate                 getSubsonicDefaultTranscodeBitrate() const { return _subsonicDefaultTranscodeBitrate; }
        std::size_t             getCurPlayingTrackPos() const { return _curPlayingTrackPos; }
        bool                    isRepeatAllSet() const { return _repeatAll; }
        bool                    isRadioSet() const { return _radio; }
        UITheme                 getUITheme() const { return _uiTheme; }
        SubsonicArtistListMode  getSubsonicArtistListMode() const { return _subsonicArtistListMode; }
        Scrobbler               getScrobbler() const { return _scrobbler; }
        std::optional<UUID>     getListenBrainzToken() const { return UUID::fromString(_listenbrainzToken); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _loginName, "login_name");
            Wt::Dbo::field(a, _passwordSalt, "password_salt");
            Wt::Dbo::field(a, _passwordHash, "password_hash");
            Wt::Dbo::field(a, _lastLogin, "last_login");
            Wt::Dbo::field(a, _subsonicDefaultTranscodeFormat, "subsonic_default_transcode_format");
            Wt::Dbo::field(a, _subsonicDefaultTranscodeBitrate, "subsonic_default_transcode_bitrate");
            Wt::Dbo::field(a, _subsonicArtistListMode, "subsonic_artist_list_mode");
            Wt::Dbo::field(a, _uiTheme, "ui_theme");
            Wt::Dbo::field(a, _scrobbler, "scrobbler");
            Wt::Dbo::field(a, _listenbrainzToken, "listenbrainz_token");

            // UI player settings
            Wt::Dbo::field(a, _curPlayingTrackPos, "cur_playing_track_pos");
            Wt::Dbo::field(a, _repeatAll, "repeat_all");
            Wt::Dbo::field(a, _radio, "radio");

            Wt::Dbo::hasMany(a, _authTokens, Wt::Dbo::ManyToOne, "user");
        }

    private:
        friend class Session;
        User(std::string_view loginName);
        static pointer create(Session& session, std::string_view loginName);

        std::string     _loginName;
        std::string     _passwordSalt;
        std::string     _passwordHash;
        Wt::WDateTime   _lastLogin;
        UITheme         _uiTheme{ defaultUITheme };
        Scrobbler       _scrobbler{ defaultScrobbler };
        std::string     _listenbrainzToken; // Musicbrainz Identifier

        // Admin defined settings
        UserType        _type{ UserType::REGULAR };

        // User defined settings
        SubsonicArtistListMode  _subsonicArtistListMode{ defaultSubsonicArtistListMode };
        AudioFormat             _subsonicDefaultTranscodeFormat{ defaultSubsonicTranscodeFormat };
        int                     _subsonicDefaultTranscodeBitrate{ defaultSubsonicTranscodeBitrate };

        // User's dynamic data (UI)
        int     _curPlayingTrackPos{}; // Current track position in queue
        bool    _repeatAll{};
        bool    _radio{};

        Wt::Dbo::collection<Wt::Dbo::ptr<AuthToken>> _authTokens;
    };

} // namespace Databas'
