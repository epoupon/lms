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

#include <Wt/WDateTime.h>

#include "core/EnumSet.hpp"

#include "database/Types.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    class User : public detail::Object<User, UserId>
    {
    public:
        struct PasswordHash
        {
            std::size_t bcryptRoundCount;
            std::string salt;
            std::string hash;
        };

        struct FindParameters
        {
            std::optional<Range> range;

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

        static User create(ISession& session, std::string_view loginName);
        static void destroy(ISession& session, std::span<const UserId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<User> find(ISession& session, UserId id);
        static std::optional<User> find(ISession& session, std::string_view loginName);
        static std::vector<UserId> findIds(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const User&)>& func);
        static std::optional<User> findDemoUser(ISession& session);
        static std::size_t getAuthTokensCount(ISession& session, UserId id);

        static void update(ISession& session, const User& user);

        // accessors
        const std::string& getLoginName() const { return _loginName; }
        PasswordHash getPasswordHash() const { return PasswordHash{ .bcryptRoundCount = static_cast<std::size_t>(_bcryptRoundCount), .salt = _passwordSalt, .hash = _passwordHash }; }
        const Wt::WDateTime& getLastLogin() const { return _lastLogin; }

        // write
        void setLastLogin(const Wt::WDateTime& dateTime) { _lastLogin = dateTime; }
        void setPasswordHash(const PasswordHash& passwordHash)
        {
            _bcryptRoundCount = passwordHash.bcryptRoundCount;
            _passwordSalt = passwordHash.salt;
            _passwordHash = passwordHash.hash;
        }
        void setType(UserType type) { _type = type; }
        void setSubsonicEnableTranscodingByDefault(bool value) { _subsonicEnableTranscodingByDefault = value; }
        void setSubsonicDefaultTranscodintOutputFormat(TranscodingOutputFormat encoding) { _subsonicDefaultTranscodingOutputFormat = encoding; }
        void setSubsonicDefaultTranscodingOutputBitrate(Bitrate bitrate);
        void setUITheme(UITheme uiTheme) { _uiTheme = uiTheme; }
        void setUIArtistReleaseSortMethod(ReleaseSortMethod method) { _uiArtistReleaseSortMethod = method; }
        void setUIEnableInlineArtistRelationships(bool enable) { _uiEnableInlineArtistRelationships = enable; }
        void setUIInlineArtistRelationships(core::EnumSet<TrackArtistLinkType> types) { _uiInlineArtistRelationships = types; }
        void setSubsonicArtistListMode(SubsonicArtistListMode mode) { _subsonicArtistListMode = mode; }
        void setFeedbackBackends(core::EnumSet<FeedbackBackend> backends);
        void setScrobblingBackends(core::EnumSet<ScrobblingBackend> backends);
        void setListenBrainzToken(std::string_view token) { _listenbrainzToken = token; }
        void setLastFmApiKey(std::string_view key) { _lastFmApiKey = key; }
        void setLastFmApiSecret(std::string_view secret) { _lastFmApiSecret = secret; }
        void setLastFmSessionKey(std::string_view key) { _lastFmSessionKey = key; }

        // read
        bool isAdmin() const { return _type == UserType::ADMIN; }
        bool isDemo() const { return _type == UserType::DEMO; }
        UserType getType() const { return _type; }
        bool getSubsonicEnableTranscodingByDefault() const { return _subsonicEnableTranscodingByDefault; }
        TranscodingOutputFormat getSubsonicDefaultTranscodingOutputFormat() const { return _subsonicDefaultTranscodingOutputFormat; }
        Bitrate getSubsonicDefaultTranscodingOutputBitrate() const { return _subsonicDefaultTranscodingOutputBitrate; }
        UITheme getUITheme() const { return _uiTheme; }
        ReleaseSortMethod getUIArtistReleaseSortMethod() const { return _uiArtistReleaseSortMethod; }
        bool getUIEnableInlineArtistRelationships() const { return _uiEnableInlineArtistRelationships; }
        core::EnumSet<TrackArtistLinkType> getUIInlineArtistRelationships() const { return _uiInlineArtistRelationships; }
        SubsonicArtistListMode getSubsonicArtistListMode() const { return _subsonicArtistListMode; }
        core::EnumSet<FeedbackBackend> getFeedbackBackends() const { return _feedbackBackends; }
        core::EnumSet<ScrobblingBackend> getScrobblingBackends() const { return _scrobblingBackends; }
        std::string_view getListenBrainzToken() const { return _listenbrainzToken; }
        std::string_view getLastFmApiKey() const { return _lastFmApiKey; }
        std::string_view getLastFmApiSecret() const { return _lastFmApiSecret; }
        std::string_view getLastFmSessionKey() const { return _lastFmSessionKey; }

    protected:
        User() = default;
        explicit User(std::string_view loginName);

        std::string _loginName;
        int _bcryptRoundCount{};
        std::string _passwordSalt;
        std::string _passwordHash;
        Wt::WDateTime _lastLogin;
        UITheme _uiTheme{ defaultUITheme };
        ReleaseSortMethod _uiArtistReleaseSortMethod{ _defaultUIArtistReleaseSortMethod };
        bool _uiEnableInlineArtistRelationships{};
        core::EnumSet<TrackArtistLinkType> _uiInlineArtistRelationships{ TrackArtistLinkType::Composer, TrackArtistLinkType::Performer };
        core::EnumSet<FeedbackBackend> _feedbackBackends;
        core::EnumSet<ScrobblingBackend> _scrobblingBackends;
        std::string _listenbrainzToken;
        std::string _lastFmApiKey;
        std::string _lastFmApiSecret;
        std::string _lastFmSessionKey;

        // Admin defined settings
        UserType _type{ UserType::REGULAR };

        // User defined settings
        SubsonicArtistListMode _subsonicArtistListMode{ defaultSubsonicArtistListMode };
        bool _subsonicEnableTranscodingByDefault{ defaultSubsonicEnableTranscodingByDefault };
        TranscodingOutputFormat _subsonicDefaultTranscodingOutputFormat{ defaultSubsonicTranscodingOutputFormat };
        int _subsonicDefaultTranscodingOutputBitrate{ defaultSubsonicTranscodingOutputBitrate };
    };
} // namespace lms::db
