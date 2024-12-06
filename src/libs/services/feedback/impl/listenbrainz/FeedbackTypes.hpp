/*
 * Copyright (C) 2022 Emeric Poupon
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

#include <iosfwd>

#include <Wt/WDateTime.h>

#include "core/UUID.hpp"

namespace lms::feedback::listenBrainz
{
    // See https://listenbrainz.readthedocs.io/en/production/dev/feedback-json/#feedback-json-doc
    enum class FeedbackType
    {
        Love = 1,
        Hate = -1,
        Erase = 0,
    };

    struct Feedback
    {
        Wt::WDateTime created;
        core::UUID recordingMBID;
        FeedbackType score;
    };

    std::ostream& operator<<(std::ostream& os, const Feedback& feedback);

} // namespace lms::feedback::listenBrainz
