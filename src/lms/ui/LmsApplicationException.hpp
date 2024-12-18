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

#include <Wt/WString.h>

#include "core/Exception.hpp"

namespace lms::ui
{
    class LmsApplicationException : public core::LmsException
    {
    public:
        LmsApplicationException(const Wt::WString& error)
            : LmsException{ error.toUTF8() } {}
    };

    class ArtistNotFoundException : public LmsApplicationException
    {
    public:
        ArtistNotFoundException()
            : LmsApplicationException{ Wt::WString::tr("Lms.Error.artist-not-found") } {}
    };

    class ReleaseNotFoundException : public LmsApplicationException
    {
    public:
        ReleaseNotFoundException()
            : LmsApplicationException{ Wt::WString::tr("Lms.Error.release-not-found") } {}
    };

    class TrackListNotFoundException : public LmsApplicationException
    {
    public:
        TrackListNotFoundException()
            : LmsApplicationException{ Wt::WString::tr("Lms.Error.tracklist-not-found") } {}
    };

    class UserNotFoundException : public LmsApplicationException
    {
    public:
        UserNotFoundException()
            : LmsApplicationException{ Wt::WString::tr("Lms.Error.user-not-found") } {}
    };

    class UserNotAllowedException : public LmsApplicationException
    {
    public:
        UserNotAllowedException()
            : LmsApplicationException{ Wt::WString::tr("Lms.Error.user-not-allowed") } {}
    };
} // namespace lms::ui
