
/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <string_view>
#include <Wt/WDate.h>

#include "metadata/IParser.hpp"

namespace MetaData::Utils
{
	Wt::WDate parseDate(const std::string& dateStr);
	std::string_view readStyleToString(ParserReadStyle readStyle);

	struct PerformerArtist
	{
		Artist artist;
		std::string role;
	};

	// format is "artist name (role)"
	PerformerArtist extractPerformerAndRole(std::string_view entry);
}

