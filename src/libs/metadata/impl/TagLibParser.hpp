/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <taglib/audioproperties.h>
#include "metadata/IParser.hpp"

namespace TagLib
{
	class StringList;
}

namespace MetaData
{

// Parse that makes use of AvFormat
class TagLibParser : public IParser
{
	public:
		TagLibParser(ParserReadStyle readStyle);

	private:
		std::optional<Track> parse(const std::filesystem::path& p, bool debug = false) override;
		void processTag(Track& track, const std::string& tag, const std::vector<std::string>& values, bool debug);

		const TagLib::AudioProperties::ReadStyle _readStyle;
};

} // namespace MetaData

