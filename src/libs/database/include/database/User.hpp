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

#include "core/UUID.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/UserId.hpp"

namespace lms::db
{
    class AuthToken;
    class Session;
    class UIState;

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
            std::optional<ScrobblingBackend> scrobblingBackend;
            std::optional<FeedbackBackend> feedbackBackend;
            std::optional<Range> range;

            FindParameters& setFeedbackBackend(FeedbackBackend _feedbackBackend)
            {
                feedbackBackend = _feedbackBackend;
                return *this;
            }
            FindParameters& setScrobblingBackend(ScrobblingBackend _scrobblingBackend)
            {
                scrobblingBackend = _scrobblingBackend;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
        };

        static inline constexpr std::size_t minNameLength{ 3 };
        static inline constexpr std::size_t maxNameLength{ 32 };
        static inline constexpr bool defaultSubsonicEnableTranscodingByDefault{ false };
        static inline constexpr TranscodingOutputFormat defaultSubsonicTranscodingOutputFormat{ TranscodingOutputFormat::OGG_OPUS };
        static inline constexpr Bitrate defaultSubsonicTranscodingOutputBitrate{ 128000 };
        static inline constexpr UITheme defaultUITheme{ UITheme::Dark };
        static inline constexpr ReleaseSortMethod _defaultUIArtistReleaseSortMethod{ ReleaseSortMethod::OriginalDateDesc };
        static inline constexpr SubsonicArtistListMode defaultSubsonicArtistListMode{ SubsonicArtistListMode::AllArtists };
        static inline constexpr ScrobblingBackend defaultScrobblingBackend{ ScrobblingBackend::Internal };
        static inline constexpr FeedbackBackend defaultFeedbackBackend{ FeedbackBackend::Internal };

        User() = default;

        static std::size_t getCount(Session& session);
        static pointer find(Session& session, UserId id);
        static pointer find(Session& session, std::string_view loginName);
        static RangeResults<UserId> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func);
        static pointer findDemoUser(Session& session);

        // accessors
        const std::string& getLoginName() const { return _loginName; }
        PasswordHash getPasswordHash() const { return PasswordHash{ _passwordSalt, _passwordHash }; }
        const Wt::WDateTime& getLastLogin() const { return _lastLogin; }
        std::size_t getAuthTokensCount() const { return _authTokens.size(); }

        // write
        void setLastLogin(const Wt::WDateTime& dateTime) { _lastLogin = dateTime; }
        void setPasswordHash(const PasswordHash& passwordHash)
        {
            _passwordSalt = passwordHash.salt;
            _passwordHash = passwordHash.hash;
        }
        void setType(UserType type) { _type = type; }
        void setSubsonicEnableTranscodingByDefault(bool value) { _subsonicEnableTranscodingByDefault = value; }
        void setSubsonicDefaultTranscodintOutputFormat(TranscodingOutputFormat encoding) { _subsonicDefaultTranscodingOutputFormat = encoding; }
        void setSubsonicDefaultTranscodingOutputBitrate(Bitrate bitrate);
        void setUITheme(UITheme uiTheme) { _uiTheme = uiTheme; }
        void setUIArtistReleaseSortMethod(ReleaseSortMethod method) { _uiArtistReleaseSortMethod = method; }
        void clearAuthTokens();
        void setSubsonicArtistListMode(SubsonicArtistListMode mode) { _subsonicArtistListMode = mode; }
        void setFeedbackBackend(FeedbackBackend feedbackBackend) { _feedbackBackend = feedbackBackend; }
        void setScrobblingBackend(ScrobblingBackend scrobblingBackend) { _scrobblingBackend = scrobblingBackend; }
        void setListenBrainzToken(const std::optional<core::UUID>& MBID) { _listenbrainzToken = MBID ? MBID->getAsString() : ""; }

        // read
        bool isAdmin() const { return _type == UserType::ADMIN; }
        bool isDemo() const { return _type == UserType::DEMO; }
        UserType getType() const { return _type; }
        bool getSubsonicEnableTranscodingByDefault() const { return _subsonicEnableTranscodingByDefault; }
        TranscodingOutputFormat getSubsonicDefaultTranscodingOutputFormat() const { return _subsonicDefaultTranscodingOutputFormat; }
        Bitrate getSubsonicDefaultTranscodingOutputBitrate() const { return _subsonicDefaultTranscodingOutputBitrate; }
        UITheme getUITheme() const { return _uiTheme; }
        ReleaseSortMethod getUIArtistReleaseSortMethod() const { return _uiArtistReleaseSortMethod; }
        SubsonicArtistListMode getSubsonicArtistListMode() const { return _subsonicArtistListMode; }
        FeedbackBackend getFeedbackBackend() const { return _feedbackBackend; }
        ScrobblingBackend getScrobblingBackend() const { return _scrobblingBackend; }
        std::optional<core::UUID> getListenBrainzToken() const { return core::UUID::fromString(_listenbrainzToken); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _loginName, "login_name");
            Wt::Dbo::field(a, _passwordSalt, "password_salt");
            Wt::Dbo::field(a, _passwordHash, "password_hash");
            Wt::Dbo::field(a, _lastLogin, "last_login");
            Wt::Dbo::field(a, _subsonicEnableTranscodingByDefault, "subsonic_enable_transcoding_by_default");
            Wt::Dbo::field(a, _subsonicDefaultTranscodingOutputFormat, "subsonic_default_transcode_format");
            Wt::Dbo::field(a, _subsonicDefaultTranscodingOutputBitrate, "subsonic_default_transcode_bitrate");
            Wt::Dbo::field(a, _subsonicArtistListMode, "subsonic_artist_list_mode");
            Wt::Dbo::field(a, _uiTheme, "ui_theme");
            Wt::Dbo::field(a, _uiArtistReleaseSortMethod, "ui_artist_release_sort_method");
            Wt::Dbo::field(a, _feedbackBackend, "feedback_backend");
            Wt::Dbo::field(a, _scrobblingBackend, "scrobbling_backend");
            Wt::Dbo::field(a, _listenbrainzToken, "listenbrainz_token");

            Wt::Dbo::hasMany(a, _authTokens, Wt::Dbo::ManyToOne, "user");
            Wt::Dbo::hasMany(a, _uiStates, Wt::Dbo::ManyToOne, "user");
        }

    private:
        friend class Session;
        User(std::string_view loginName);
        static pointer create(Session& session, std::string_view loginName);

        std::string _loginName;
        std::string _passwordSalt;
        std::string _passwordHash;
        Wt::WDateTime _lastLogin;
        UITheme _uiTheme{ defaultUITheme };
        ReleaseSortMethod _uiArtistReleaseSortMethod{ _defaultUIArtistReleaseSortMethod };
        FeedbackBackend _feedbackBackend{ defaultFeedbackBackend };
        ScrobblingBackend _scrobblingBackend{ defaultScrobblingBackend };
        std::string _listenbrainzToken; // Musicbrainz Identifier

        // Admin defined settings
        UserType _type{ UserType::REGULAR };

        // User defined settings
        SubsonicArtistListMode _subsonicArtistListMode{ defaultSubsonicArtistListMode };
        bool _subsonicEnableTranscodingByDefault{ defaultSubsonicEnableTranscodingByDefault };
        TranscodingOutputFormat _subsonicDefaultTranscodingOutputFormat{ defaultSubsonicTranscodingOutputFormat };
        int _subsonicDefaultTranscodingOutputBitrate{ defaultSubsonicTranscodingOutputBitrate };

        Wt::Dbo::collection<Wt::Dbo::ptr<AuthToken>> _authTokens;
        Wt::Dbo::collection<Wt::Dbo::ptr<UIState>> _uiStates;
    };
} // namespace lms::db
