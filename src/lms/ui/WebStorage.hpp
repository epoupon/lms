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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Wt/WJavaScript.h>
#include <Wt/WSignal.h>

#include "core/UUID.hpp"

#include "database/objects/UserId.hpp"

namespace Wt
{
    class WApplication;
    class WObject;
} // namespace Wt

namespace lms::ui
{
    // Wrapper around the browser's HTML5 localStorage.
    // Keys are prefixed with the server instance and the user: a browser keeps one storage for all the LMS instances
    // behind the same origin, and for all the users who log in from it
    class WebStorage
    {
    public:
        WebStorage(Wt::WApplication& app, const core::UUID& serverInstanceId, db::UserId userId);
        ~WebStorage() = default;
        WebStorage(const WebStorage&) = delete;
        WebStorage& operator=(const WebStorage&) = delete;

        void setItem(std::string_view key, std::string_view value);
        void removeItem(std::string_view key);

        // "owner" is used to auto-cancel the read if it no longer exists once the value comes back
        using ItemCallback = std::function<void(std::optional<std::string> value)>;
        void getItem(const Wt::WObject& owner, std::string_view key, ItemCallback cb);

    private:
        std::string getLocalStorageKey(std::string_view key) const;

        Wt::WApplication& _app;
        const std::string _keyPrefix;
        Wt::JSignal<int, bool, std::string> _itemLoaded; // requestId, found, value
        std::unordered_map<int, std::unique_ptr<Wt::Signal<bool, std::string>>> _pendingRequests;
        int _nextRequestId{};
    };
} // namespace lms::ui
