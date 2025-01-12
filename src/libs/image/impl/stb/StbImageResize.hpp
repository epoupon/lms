/*
 * Copyright (C) 2020 Emeric Poupon
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

#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_MITCHELL
#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_CATMULLROM

#if STB_IMAGE_RESIZE_VERSION == 1
    #include <stb_image_resize.h>
#elif STB_IMAGE_RESIZE_VERSION == 2
    #include <stb_image_resize2.h>
#else
    #error "Unhandled STB image resize version"!
#endif
