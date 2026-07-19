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

#include <chrono>
#include <string>
#include <vector>

#include <Wt/AsioWrapper/system_error.hpp>
#include <Wt/Http/Message.h>
#include <Wt/WSignal.h>

namespace lms::core::http
{
    // Mirrors Wt::Http::Client's surface as used by SendQueue, so SendQueue's retry/throttle/
    // completeness logic can be driven by a fake in tests instead of real network I/O.
    class IWtHttpClient
    {
    public:
        virtual ~IWtHttpClient() = default;

        virtual void setFollowRedirect(bool enable) = 0;
        virtual void setTimeout(std::chrono::steady_clock::duration timeout) = 0;
        virtual void setMaximumResponseSize(std::size_t bytes) = 0;

        virtual bool get(const std::string& url, const std::vector<Wt::Http::Message::Header>& headers) = 0;
        virtual bool post(const std::string& url, const Wt::Http::Message& message) = 0;
        virtual void abort() = 0;

        virtual Wt::Signal<std::string>& bodyDataReceived() = 0;
        virtual Wt::Signal<Wt::AsioWrapper::error_code, Wt::Http::Message>& done() = 0;
    };
} // namespace lms::core::http
