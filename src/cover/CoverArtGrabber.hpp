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
#include "database/Types.hpp"

#include "CoverArt.hpp"


namespace CoverArt {

class Grabber
{
	public:
		Grabber(const Grabber&) = delete;
		Grabber& operator=(const Grabber&) = delete;

		static Grabber& instance();

		void init();

		std::vector<boost::filesystem::path>	getCoverPaths(const boost::filesystem::path& directoryPath, std::size_t nbMaxCovers = 1) const;
		std::vector<CoverArt>	getFromDirectory(const boost::filesystem::path& path, std::size_t nbMaxCovers = 1) const;
		std::vector<CoverArt>	getFromInputFormatContext(const Av::InputFormatContext& input, std::size_t nbMaxCovers = 1) const;
		std::vector<CoverArt>	getFromTrack(const boost::filesystem::path& path, std::size_t nbMaxCovers = 1) const;
		std::vector<CoverArt>	getFromTrack(Wt::Dbo::Session& session, Database::Track::id_type trackId, std::size_t nbMaxCovers = 1) const;
		std::vector<CoverArt>	getFromRelease(Wt::Dbo::Session& session, Database::Release::id_type releaseId, std::size_t nbMaxCovers = 1) const;

	private:
		Grabber();

		std::vector<boost::filesystem::path> _fileExtensions;
		std::size_t _maxFileSize;
		std::vector<boost::filesystem::path> _preferredFileNames;


};

} // namespace CoverArt


#endif
