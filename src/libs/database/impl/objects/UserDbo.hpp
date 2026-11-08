/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/objects/User.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class AuthToken;
    class UIState;

    class User final : public db::User, public Wt::Dbo::Dbo<User>
    {
    public:
        User() = default;
        using db::User::User;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _type, "type");
            Wt::Dbo::field(a, _loginName, "login_name");
            Wt::Dbo::field(a, _bcryptRoundCount, "bcrypt_round_count");
            Wt::Dbo::field(a, _passwordSalt, "password_salt");
            Wt::Dbo::field(a, _passwordHash, "password_hash");
            Wt::Dbo::field(a, _lastLogin, "last_login");
            Wt::Dbo::field(a, _subsonicEnableTranscodingByDefault, "subsonic_enable_transcoding_by_default");
            Wt::Dbo::field(a, _subsonicDefaultTranscodingOutputFormat, "subsonic_default_transcode_format");
            Wt::Dbo::field(a, _subsonicDefaultTranscodingOutputBitrate, "subsonic_default_transcode_bitrate");
            Wt::Dbo::field(a, _subsonicArtistListMode, "subsonic_artist_list_mode");
            Wt::Dbo::field(a, _uiTheme, "ui_theme");
            Wt::Dbo::field(a, _uiArtistReleaseSortMethod, "ui_artist_release_sort_method");
            Wt::Dbo::field(a, _uiEnableInlineArtistRelationships, "ui_enable_inline_artist_relationships");
            Wt::Dbo::field(a, _uiInlineArtistRelationships, "ui_inline_artist_relationships");
            Wt::Dbo::field(a, _feedbackBackends, "feedback_backends");
            Wt::Dbo::field(a, _scrobblingBackends, "scrobbling_backends");
            Wt::Dbo::field(a, _listenbrainzToken, "listenbrainz_token");
            Wt::Dbo::field(a, _lastFmApiKey, "lastfm_api_key");
            Wt::Dbo::field(a, _lastFmApiSecret, "lastfm_api_secret");
            Wt::Dbo::field(a, _lastFmSessionKey, "lastfm_session_key");

            Wt::Dbo::hasMany(a, _authTokens, Wt::Dbo::ManyToOne, "user");
            Wt::Dbo::hasMany(a, _uiStates, Wt::Dbo::ManyToOne, "user");
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<AuthToken>> _authTokens;
        Wt::Dbo::collection<Wt::Dbo::ptr<UIState>> _uiStates;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<User>
    {
        using type = dbo::User;
    };
} // namespace lms::db
