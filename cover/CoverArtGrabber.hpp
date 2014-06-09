#ifndef COVER_ART_GRABBER_HPP
#define COVER_ART_GRABBER_HPP

#include "CoverArt.hpp"


namespace CoverArt {

class Grabber
{
	public:

		static std::vector<CoverArt>	getFromTrack(Track::pointer	track);
		static std::vector<CoverArt>	getFromRelease(Release::pointer	release);
};

} // namespace CoverArt


#endif
