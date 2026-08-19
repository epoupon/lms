/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "SettingsView.hpp"

#include "core/IConfig.hpp"
#include "core/Service.hpp"

#include "AudioSettingsView.hpp"
#include "LastFmSettingsView.hpp"
#include "ListenBrainzSettingsView.hpp"
#include "LmsApplication.hpp"
#include "PasswordSettingsView.hpp"
#include "SubsonicSettingsView.hpp"
#include "UISettingsView.hpp"
#include "common/PathRouter.hpp"

namespace lms::ui
{
    SettingsView::SettingsView()
    {
        auto* router{ addNew<PathRouter>() };

        router->add<UISettingsView>("/settings/ui", Wt::WString::tr("Lms.Settings.user-interface"));
        router->add<AudioSettingsView>("/settings/audio", Wt::WString::tr("Lms.Settings.audio"));

        if (core::Service<core::IConfig>::get()->getBool("api-subsonic", true))
            router->add<SubsonicSettingsView>("/settings/subsonic", Wt::WString::tr("Lms.Settings.subsonic-api"));

        router->add<ListenBrainzSettingsView>("/settings/listenbrainz", Wt::WString::tr("Lms.Settings.listenbrainz"));
        router->add<LastFmSettingsView>("/settings/lastfm", Wt::WString::tr("Lms.Settings.lastfm"));

        if (LmsApp->getAuthBackend() == AuthenticationBackend::Internal)
            router->add<PasswordSettingsView>("/settings/password", Wt::WString::tr("Lms.Settings.change-password"));

        router->activate();
    }
} // namespace lms::ui
