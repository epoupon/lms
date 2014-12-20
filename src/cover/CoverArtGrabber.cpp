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


#include <boost/foreach.hpp>

#include "logger/Logger.hpp"
#include "av/InputFormatContext.hpp"

#include "CoverArtGrabber.hpp"



namespace CoverArt {


std::vector<CoverArt>
Grabber::getFromInputFormatContext(const Av::InputFormatContext& input)
{
	std::vector<CoverArt> res;

	try
	{
		std::vector<Av::Picture> pictures;
		input.getPictures(pictures);

		BOOST_FOREACH(const Av::Picture& picture, pictures)
			res.push_back( CoverArt(picture.mimeType, picture.data) );

	}
	catch(std::exception& e)
	{
		LMS_LOG(MOD_COVER, SEV_ERROR) << "Cannot get pictures: " << e.what();
	}

	return res;
}

std::vector<CoverArt>
Grabber::getFromTrack(const boost::filesystem::path& p)
{
	std::vector<CoverArt> res;

	try
	{
		Av::InputFormatContext input(p);

		return getFromInputFormatContext(input);

	}
	catch(std::exception& e)
	{
		LMS_LOG(MOD_COVER, SEV_ERROR) << "Cannot get pictures: " << e.what();
	}

	return res;
}



std::vector<CoverArt>
Grabber::getFromTrack(Database::Track::pointer track)
{
	std::vector<CoverArt> res;

	if (!track || !track->hasCover())
		return std::vector<CoverArt>();

	try
	{
		Av::InputFormatContext input(track->getPath());

		return getFromInputFormatContext(input);
	}
	catch(std::exception& e)
	{
		LMS_LOG(MOD_COVER, SEV_ERROR) << "Cannot get pictures: " << e.what();
	}

	return res;
}

} // namespace CoverArt

