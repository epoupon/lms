/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "metadata/IParser.hpp"

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include "AvFormatParser.hpp"
#include "TagLibParser.hpp"
#include "Utils.hpp"

namespace MetaData
{
	std::unique_ptr<IParser>
	createParser(ParserType parserType, ParserReadStyle parserReadStyle)
	{
		switch (parserType)
		{
			case ParserType::TagLib:
				LMS_LOG(METADATA, INFO) << "Creating TagLib parser with read style = " << Utils::readStyleToString(parserReadStyle);
				return std::make_unique<TagLibParser>(parserReadStyle);
			case ParserType::AvFormat:
				LMS_LOG(METADATA, INFO) << "Creating AvFormat parser";
				return std::make_unique<AvFormatParser>();
		}

		throw LmsException {"Unhandled parser type"};
	}
}

