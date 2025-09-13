/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <atomic>
#include <filesystem>
#include <functional>

#include "core/LiteralString.hpp"

#include "RefreshContext.hpp"

namespace lms::podcast
{
    class RefreshStep
    {
    public:
        using OnDoneCallback = std::function<void(bool success)>;

        RefreshStep(RefreshContext& context, OnDoneCallback callback)
            : _context{ context }
            , _onDoneCallback{ std::move(callback) } {}
        virtual ~RefreshStep() = default;

        virtual core::LiteralString getName() const = 0;
        virtual void run() = 0;

        void requestAbort(bool value)
        {
            _abortRequested = value;
        }

    protected:
        bool abortRequested() const
        {
            return _abortRequested;
        }

        // Called by the step implementation when done
        void onDone()
        {
            _onDoneCallback(true);
        }

        // Called by the step implementation when it wants to abort the whole refresh process
        void onAbort()
        {
            _onDoneCallback(false);
        }

        Executor& getExecutor()
        {
            return _context.executor;
        }

        db::IDb& getDb()
        {
            return _context.db;
        }

        const std::filesystem::path& getCachePath() const
        {
            return _context.cachePath;
        }

        const std::filesystem::path& getTmpCachePath() const
        {
            return _context.tmpCachePath;
        }

        core::http::IClient& getClient()
        {
            return _context.client;
        }

    private:
        RefreshContext& _context;
        OnDoneCallback _onDoneCallback;
        std::atomic<bool> _abortRequested;
    };

} // namespace lms::podcast