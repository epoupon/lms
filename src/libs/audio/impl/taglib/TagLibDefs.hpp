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

static_assert(TAGLIB_MAJOR_VERSION >= 2);

#define LMS_TAGLIB_HAS_MP4_ITEM_TYPE (((TAGLIB_MAJOR_VERSION > 2) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_MINOR_VERSION > 0) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_PATCH_VERSION >= 1))) // >= 2.0.1

#define LMS_TAGLIB_HAS_APE_COMPLEX_PROPERTIES (((TAGLIB_MAJOR_VERSION > 2) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_MINOR_VERSION > 0) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_PATCH_VERSION >= 2))) // >= 2.0.2

// taglib_config.h only exists from version >= 2.1
#if ((TAGLIB_MAJOR_VERSION > 2) || (TAGLIB_MAJOR_VERSION == 2 && TAGLIB_MINOR_VERSION >= 1))
    #include <taglib/taglib_config.h>
    #define LMS_TAGLIB_HAS_BUILD_CONFIG 1
#else
    #define LMS_TAGLIB_HAS_BUILD_CONFIG 0
#endif

// Shorten support and taglib_config.h were both added within the same release
#if LMS_TAGLIB_HAS_BUILD_CONFIG && defined(TAGLIB_WITH_SHORTEN)
    #define LMS_TAGLIB_HAS_SHORTEN 1
#else
    #define LMS_TAGLIB_HAS_SHORTEN 0
#endif

#if LMS_TAGLIB_HAS_BUILD_CONFIG
    #if defined(TAGLIB_WITH_RIFF)
        #define LMS_TAGLIB_HAS_RIFF 1
    #else
        #define LMS_TAGLIB_HAS_RIFF 0
    #endif
    #if defined(TAGLIB_WITH_APE)
        #define LMS_TAGLIB_HAS_APE 1
    #else
        #define LMS_TAGLIB_HAS_APE 0
    #endif
    #if defined(TAGLIB_WITH_ASF)
        #define LMS_TAGLIB_HAS_ASF 1
    #else
        #define LMS_TAGLIB_HAS_ASF 0
    #endif
    #if defined(TAGLIB_WITH_VORBIS)
        #define LMS_TAGLIB_HAS_VORBIS 1
    #else
        #define LMS_TAGLIB_HAS_VORBIS 0
    #endif
    #if defined(TAGLIB_WITH_MP4)
        #define LMS_TAGLIB_HAS_MP4 1
    #else
        #define LMS_TAGLIB_HAS_MP4 0
    #endif
    #if defined(TAGLIB_WITH_TRUEAUDIO)
        #define LMS_TAGLIB_HAS_TRUEAUDIO 1
    #else
        #define LMS_TAGLIB_HAS_TRUEAUDIO 0
    #endif
    #if defined(TAGLIB_WITH_DSF)
        #define LMS_TAGLIB_HAS_DSF 1
    #else
        #define LMS_TAGLIB_HAS_DSF 0
    #endif
#else
    #define LMS_TAGLIB_HAS_RIFF 1
    #define LMS_TAGLIB_HAS_APE 1
    #define LMS_TAGLIB_HAS_ASF 1
    #define LMS_TAGLIB_HAS_VORBIS 1
    #define LMS_TAGLIB_HAS_MP4 1
    #define LMS_TAGLIB_HAS_TRUEAUDIO 1
    #define LMS_TAGLIB_HAS_DSF 1
#endif
