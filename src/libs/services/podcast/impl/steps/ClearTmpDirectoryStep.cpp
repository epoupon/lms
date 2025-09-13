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

#include "ClearTmpDirectoryStep.hpp"

#include "core/ILogger.hpp"

namespace lms::podcast
{
    namespace
    {
        bool clearDirectory(const std::filesystem::path& _rootPath)
        {
            for (const auto& entry : std::filesystem::directory_iterator{ _rootPath })
            {
                std::error_code ec;
                std::filesystem::remove_all(entry, ec);
                if (ec)
                {
                    LMS_LOG(PODCAST, ERROR, "Failed to remove " << entry << ": " << ec.message());
                    return false;
                }
            }

            return true;
        }
    } // namespace

    core::LiteralString ClearTmpDirectoryStep::getName() const
    {
        return "Clear tmp Directory";
    }

    void ClearTmpDirectoryStep::run()
    {
        if (!clearDirectory(getTmpCachePath()))
        {
            LMS_LOG(PODCAST, ERROR, "Failed to delete tmp directory " << getTmpCachePath() << ": aborting refresh");
            onAbort();
            return;
        }

        onDone();
    }

} // namespace lms::podcast