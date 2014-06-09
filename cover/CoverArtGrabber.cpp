
#include <boost/foreach.hpp>

#include "CoverArtGrabber.hpp"

#include "av/InputFormatContext.hpp"


namespace CoverArt {

std::vector<CoverArt>
Grabber::getFromRelease(Release::pointer release)
{
	std::vector<CoverArt> res;

	// TODO
	// Check if there is an image file in the directory of the release
	// For now, just get the cover art from the first track of the release

	Wt::Dbo::collection<Track::pointer> tracks (release->getTracks());

	Track::pointer firstTrack;
	if (tracks.begin() != tracks.end())
		firstTrack = *tracks.begin();

	if (firstTrack)
	{
		try
		{
			Av::InputFormatContext input(firstTrack->getPath());

			std::vector< std::vector<unsigned char> > pictures;
			input.getPictures(pictures);

			BOOST_FOREACH(const std::vector<unsigned char>& picture, pictures)
				res.push_back( CoverArt("application/octet-stream", picture) );
		}
		catch(std::exception& e)
		{
			std::cerr << "Cannot get pictures: " << e.what();
		}
	}

	return res;

}

} // namespace CoverArt

