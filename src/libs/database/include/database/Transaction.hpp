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

#include <mutex>

#include "core/ITraceLogger.hpp"

namespace lms::core
{
    class RecursiveSharedMutex;
}

namespace lms::db
{
    class Session;

    class WriteTransaction
    {
    public:
        ~WriteTransaction() noexcept(false);

    private:
        friend class Session;
        WriteTransaction(core::RecursiveSharedMutex& mutex, Session& session);

        WriteTransaction(const WriteTransaction&) = delete;
        WriteTransaction& operator=(const WriteTransaction&) = delete;

        Session& _session;
        const std::unique_lock<core::RecursiveSharedMutex> _lock;
        const core::tracing::ScopedTrace _trace;
    };

    class ReadTransaction
    {
    public:
        ~ReadTransaction() noexcept(false);

    private:
        friend class Session;
        explicit ReadTransaction(Session& session);

        ReadTransaction(const ReadTransaction&) = delete;
        ReadTransaction& operator=(const ReadTransaction&) = delete;

        Session& _session;
        const core::tracing::ScopedTrace _trace;
    };
} // namespace lms::db
