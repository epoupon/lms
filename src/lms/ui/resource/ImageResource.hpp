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

#ifndef COVER_RESOURCE_HPP_
#define COVER_RESOURCE_HPP_

#include <mutex>

#include <Wt/WResource.h>

#include "database/Types.hpp"

namespace UserInterface {


class ImageResource : public Wt::WResource
{
	public:
		static const std::size_t maxSize {512};

		ImageResource();
		~ImageResource();

		enum class Size : std::size_t
		{
			Small = 128,
			Large = 512,
		};

		std::string getReleaseUrl(Database::IdType releaseId, Size size) const;
		std::string getTrackUrl(Database::IdType trackId, Size size) const;

		static std::string getMimeType();

		void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response);

};

} // namespace UserInterface

#endif
