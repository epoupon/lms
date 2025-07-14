/*
 * Copyright (C) 2014 Emeric Poupon
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

#include <Wt/WResource.h>

#include "database/objects/ArtworkId.hpp"

namespace lms::ui
{
    class ArtworkResource : public Wt::WResource
    {
    public:
        static const std::size_t maxSize{ 512 };

        ArtworkResource();
        ~ArtworkResource() override;

        enum class Size : std::size_t
        {
            Small = 128,
            Large = 512,
        };

        enum class DefaultArtworkType
        {
            Artist,
            Release,
            Track,
        };

        std::string getArtworkUrl(db::ArtworkId artworkId, DefaultArtworkType fallbackType, std::optional<Size> size = std::nullopt) const;
        std::string getDefaultArtworkUrl(DefaultArtworkType type) const;

    private:
        void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;
    };
} // namespace lms::ui
