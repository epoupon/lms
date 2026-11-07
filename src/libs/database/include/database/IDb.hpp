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

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>

#include "database/objects/Types.hpp"

namespace lms::db
{
    class ISession;
    class IDb
    {
    public:
        using ProgressCallback = std::function<void(std::size_t processedEntries, std::size_t totalEntries)>;
        using ShouldAbortCallback = std::function<bool()>;

        virtual ~IDb() = default;

        virtual ISession& getTLSSession() = 0;
        virtual std::unique_ptr<ISession> createSession() = 0;

        // The methods below open their own transactions: none must be open on the calling thread

        // To be called once at startup, before any other use of the database
        virtual void prepare(RecommendationEngineType defaultRecommendationEngineType) = 0;
        virtual void analyze(const ProgressCallback& onProgress = {}, const ShouldAbortCallback& shouldAbort = {}) = 0;
        virtual void vacuum() = 0;
    };

    std::unique_ptr<IDb> createDb(const std::filesystem::path& dbPath);
} // namespace lms::db
