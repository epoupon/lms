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
		std::vector< std::vector<unsigned char> > pictures;
		input.getPictures(pictures);

		BOOST_FOREACH(const std::vector<unsigned char>& picture, pictures)
			res.push_back( CoverArt("application/octet-stream", picture) );

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

	if (!track)
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

std::vector<CoverArt>
Grabber::getFromRelease(Database::Release::pointer release)
{
	if (!release)
		return std::vector<CoverArt>();

	// TODO
	// Check if there is an image file in the directory of the release
	// For now, just get the cover art from the first track of the release

	Wt::Dbo::collection<Database::Track::pointer> tracks (release->getTracks());

	Database::Track::pointer firstTrack;
	if (tracks.begin() != tracks.end())
		firstTrack = *tracks.begin();

	if (firstTrack)
	{
		return Grabber::getFromTrack(firstTrack);
	}
	else
		return std::vector<CoverArt>();

}

} // namespace CoverArt

