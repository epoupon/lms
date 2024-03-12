/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "av/RawResourceHandlerCreator.hpp"

#include "av/IAudioFile.hpp"
#include "core/FileResourceHandlerCreator.hpp"

namespace lms::av
{
    std::unique_ptr<IResourceHandler> createRawResourceHandler(const std::filesystem::path& path)
    {
        std::string_view mimeType{ getMimeType(path.extension()) };
        return createFileResourceHandler(path, mimeType.empty() ? "application/octet-stream" : mimeType);
    }
}
