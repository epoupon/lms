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

#include "WebStorage.hpp"

#include <sstream>

#include <Wt/WApplication.h>
#include <Wt/WObject.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::ui
{
    WebStorage::WebStorage(Wt::WApplication& app)
        : _app{ app }
        , _itemLoaded{ &app, "webStorageItemLoaded" }
    {
        _itemLoaded.connect([this](int requestId, bool found, const std::string& value) {
            const auto it{ _pendingRequests.find(requestId) };
            if (it == _pendingRequests.end())
                return; // already cancelled (owner destroyed) or unknown

            it->second->emit(found, value);
            _pendingRequests.erase(it);
        });
    }

    void WebStorage::setItem(std::string_view key, std::string_view value)
    {
        std::ostringstream oss;
        oss << "localStorage.setItem('" << core::stringUtils::jsEscape(key) << "', '" << core::stringUtils::jsEscape(value) << "')";

        LMS_LOG(UI, DEBUG, "Running js = '" << oss.str() << "'");
        _app.doJavaScript(oss.str());
    }

    void WebStorage::removeItem(std::string_view key)
    {
        std::ostringstream oss;
        oss << "localStorage.removeItem('" << core::stringUtils::jsEscape(key) << "')";

        LMS_LOG(UI, DEBUG, "Running js = '" << oss.str() << "'");
        _app.doJavaScript(oss.str());
    }

    void WebStorage::getItem(const Wt::WObject& owner, std::string_view key, ItemCallback cb)
    {
        const int requestId{ _nextRequestId++ };

        auto signal{ std::make_unique<Wt::Signal<bool, std::string>>() };
        signal->connect(&owner, [cb = std::move(cb)](bool found, const std::string& value) {
            cb(found ? std::make_optional(value) : std::nullopt);
        });
        _pendingRequests.emplace(requestId, std::move(signal));

        const std::string keyEscaped{ core::stringUtils::jsEscape(key) };

        std::ostringstream oss;
        oss << "(function(){"
            << " var v = localStorage.getItem('" << keyEscaped << "');"
            << _itemLoaded.createCall({ std::to_string(requestId), "v !== null", "v || ''" })
            << "})()";

        LMS_LOG(UI, DEBUG, "Running js = '" << oss.str() << "'");
        _app.doJavaScript(oss.str());
    }
} // namespace lms::ui
