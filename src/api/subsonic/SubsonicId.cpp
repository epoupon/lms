/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "SubsonicId.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace API::Subsonic
{

boost::optional<Id>
IdFromString(const std::string& id)
{
	std::vector<std::string> values {splitString(id, "-")};
	if (values.size() != 2)
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Bad id format";
		return {};
	}

	Id res;

	std::string type {std::move(values[0])};
	if (type == "artist")
		res.type = Id::Type::Artist;
	else if (type == "album")
		res.type = Id::Type::Release;
	else if (type == "track")
		res.type = Id::Type::Track;
	else
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Bad id format";
		return {};
	}

	auto optId {readAs<Database::IdType>(values[1])};
	if (!optId)
	{
		LMS_LOG(API_SUBSONIC, ERROR) << "Bad id format";
		return {};
	}

	res.id = *optId;

	return res;
}

std::string
IdToString(const Id& id)
{
	std::string res;

	switch (id.type)
	{
		case Id::Type::Artist:
			res = "artist-";
			break;
		case Id::Type::Release:
			res = "album-";
			break;
		case Id::Type::Track:
			res = "track-";
			break;
	}

	return res + std::to_string(id.id);
}

} // namespace API::Subsonic
