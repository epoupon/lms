/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "services/auth/IEnvService.hpp"

#include "services/auth/Types.hpp"

#include "http-headers/HttpHeadersEnvService.hpp"

namespace lms::auth
{
    std::unique_ptr<IEnvService> createEnvService(std::string_view backendName, db::IDb& db)
    {
        if (backendName == "http-headers")
            return std::make_unique<HttpHeadersEnvService>(db);

        throw Exception{ "Authentication backend '" + std::string{ backendName } + "' is not supported!" };
    }
} // namespace lms::auth
