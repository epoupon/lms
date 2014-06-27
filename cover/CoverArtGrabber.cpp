
#include <boost/foreach.hpp>

#include "CoverArtGrabber.hpp"

#include "av/InputFormatContext.hpp"


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
		std::cerr << "Cannot get pictures: " << e.what() << std::endl;
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
		std::cerr << "Cannot get pictures: " << e.what();
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

