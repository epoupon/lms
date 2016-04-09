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

#include <Wt/WResource>

#include "database/DatabaseHandler.hpp"
#include "image/Image.hpp"

namespace UserInterface {


class ImageResource : public Wt::WResource
{
	public:
		static const std::size_t maxSize = 512;

		ImageResource(Database::Handler& db, Wt::WObject *parent = 0);
		~ImageResource();

		std::string getReleaseUrl(Database::Release::id_type releaseId, size_t size) const;
		std::string getTrackUrl(Database::Track::id_type trackId, size_t size) const;
		std::string getArtistUrl(Database::Artist::id_type artistId, size_t size) const;
		std::string getUnknownTrackUrl(size_t size) const;

		void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response);

	private:

		Image::Image			getDefaultCover(std::size_t size);
		Image::Image			getDefaultArtistImage(std::size_t size);
		void				putImage(Wt::Http::Response& response, Image::Image image);
		void				putCover(Wt::Http::Response& response, std::vector<Image::Image> covers, std::size_t size);

		// Used to protect transactions since they are not thread safe
		std::mutex			_mutex;
		Database::Handler&		_db;

		// Default cover for different sizes
		std::map<std::size_t, Image::Image>	_defaultCovers;

		// Default artist images for different sizes
		std::map<std::size_t, Image::Image>	_defaultArtistImages;

		// TODO construct a cache for covers?
};

} // namespace UserInterface

#endif
