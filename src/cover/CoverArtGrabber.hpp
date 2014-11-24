/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef COVER_ART_GRABBER_HPP
#define COVER_ART_GRABBER_HPP

#include <vector>

#include "av/InputFormatContext.hpp"
#include "database/AudioTypes.hpp"

#include "CoverArt.hpp"


namespace CoverArt {

class Grabber
{
	public:

		static std::vector<CoverArt>	getFromInputFormatContext(const Av::InputFormatContext& input);
		static std::vector<CoverArt>	getFromTrack(Database::Track::pointer		track);
};

} // namespace CoverArt


#endif
