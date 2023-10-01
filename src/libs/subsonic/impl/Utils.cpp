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

#include "Utils.hpp"

#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "services/auth/IPasswordService.hpp"
#include "SubsonicResponse.hpp"

namespace API::Subsonic::Utils
{
    void checkSetPasswordImplemented()
    {
        Auth::IPasswordService* passwordService{ Service<Auth::IPasswordService>::get() };
        if (!passwordService || !passwordService->canSetPasswords())
            throw NotImplementedGenericError{};
    }

    std::string makeNameFilesystemCompatible(const std::string& name)
    {
        return StringUtils::replaceInString(name, "/", "_");
    }

}