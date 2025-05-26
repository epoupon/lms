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

#include <taglib/taglib.h>

#if (TAGLIB_MAJOR_VERSION > 2)
    #define TAGLIB_HAS_DSF 1
#endif

// TAGLIB_HAS_MP4_ITEM_TYPE if version >= 2.0.1
#if ((TAGLIB_MAJOR_VERSION > 2) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_MINOR_VERSION > 0) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_PATCH_VERSION >= 1))
    #define TAGLIB_HAS_MP4_ITEM_TYPE 1
#endif

// TAGLIB_HAS_APE_COMPLEX_PROPERTIES if version >= 2.0.2
#if ((TAGLIB_MAJOR_VERSION > 2) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_MINOR_VERSION > 0) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_PATCH_VERSION >= 2))
    #define TAGLIB_HAS_APE_COMPLEX_PROPERTIES 1
#endif
