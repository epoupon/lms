/*
 * Copyright (C) 2019 Emeric Poupon
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
#include <unordered_map>

#include "core/NetAddress.hpp" // for unordered_map of boost::asio::ip::address

namespace lms::auth
{
    class LoginThrottler
    {
    public:
        using Clock = std::chrono::steady_clock;

        LoginThrottler(std::size_t maxEntries)
            : _maxEntries{ maxEntries } {}

        ~LoginThrottler() = default;
        LoginThrottler(const LoginThrottler&) = delete;
        LoginThrottler& operator=(const LoginThrottler&) = delete;

        // user must lock these calls to avoid races
        bool isClientThrottled(const boost::asio::ip::address& address) const;
        void onBadClientAttempt(const boost::asio::ip::address& address);
        void onGoodClientAttempt(const boost::asio::ip::address& address);

    private:
        void removeOutdatedEntries();

        const std::size_t _maxEntries;
        static constexpr std::size_t _maxBadConsecutiveAttemptCount{ 5 };
        static constexpr std::chrono::seconds _throttlingDuration{ 3 };

        struct AttemptInfo
        {
            Clock::time_point nextAttempt{};
            std::size_t badConsecutiveAttemptCount{};
        };
        std::unordered_map<boost::asio::ip::address, AttemptInfo> _attemptsInfo;
    };
} // namespace lms::auth
