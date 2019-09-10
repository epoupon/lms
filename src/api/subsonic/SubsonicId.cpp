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

#include "SubsonicResponse.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace API::Subsonic
{

std::optional<Id>
IdFromString(const std::string& id)
{
	if (id == "root")
		return Id {Id::Type::Root};

	std::vector<std::string> values {splitString(id, "-")};
	if (values.size() != 2)
		return std::nullopt;

	Id res;

	const std::string type {std::move(values[0])};
	if (type == "ar")
		res.type = Id::Type::Artist;
	else if (type == "al")
		res.type = Id::Type::Release;
	else if (type == "tr")
		res.type = Id::Type::Track;
	else if (type == "pl")
		res.type = Id::Type::Playlist;
	else
		return std::nullopt;

	auto optId {readAs<Database::IdType>(values[1])};
	if (!optId)
		return std::nullopt;

	res.value = *optId;

	return res;
}

std::string
IdToString(const Id& id)
{
	std::string res;

	switch (id.type)
	{
		case Id::Type::Root:
			return "root";
		case Id::Type::Artist:
			res = "ar-";
			break;
		case Id::Type::Release:
			res = "al-";
			break;
		case Id::Type::Track:
			res = "tr-";
			break;
		case Id::Type::Playlist:
			res = "pl-";
			break;
	}

	return res + std::to_string(id.value);
}

} // namespace API::Subsonic
