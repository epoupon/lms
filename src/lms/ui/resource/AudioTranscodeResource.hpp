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

#include <Wt/Dbo/ptr.h>
#include <Wt/WResource.h>

#include "services/database/TrackId.hpp"

namespace Database
{
	class User;
}

namespace UserInterface
{
	class AudioTranscodeResource : public Wt::WResource
	{
		public:
			~AudioTranscodeResource();

			// Url depends on the user since settings are used in parameters
			std::string getUrl(Database::TrackId trackId) const;

			void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response);

		private:
			static constexpr std::size_t	_chunkSize {262144};
	};
} // namespace UserInterface




