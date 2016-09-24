
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

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>

#pragma once

namespace Feature {

class Extractor
{
	public:
		static bool init(void);

		static bool getLowLevel(boost::property_tree::ptree& pt, boost::filesystem::path path);
		static bool getLowLevel(boost::property_tree::ptree& pt, std::string mbid);

		static bool getHighLevel(boost::property_tree::ptree& pt, std::string mbid);

	private:
		Extractor();
};

} // namespace Feature

