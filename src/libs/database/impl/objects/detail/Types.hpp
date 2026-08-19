/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <optional>

#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"
#include "core/media/ImageFormat.hpp"
#include "core/media/ImageType.hpp"

#include "database/objects/detail/Types.hpp"

namespace lms::db::detail
{
    std::optional<core::media::Container> getMediaContainerType(db::detail::Container container);
    db::detail::Container getDbContainer(core::media::Container container);

    std::optional<core::media::Codec> getMediaCodecType(db::detail::Codec codec);
    db::detail::Codec getDbCodec(core::media::Codec codec);

    core::media::ImageType getMediaImageType(db::detail::ImageType type);
    db::detail::ImageType getDbImageType(core::media::ImageType type);

    std::optional<core::media::ImageFormat> getMediaImageFormat(db::detail::ImageFormat format);
    db::detail::ImageFormat getDbImageFormat(core::media::ImageFormat format);
} // namespace lms::db::detail