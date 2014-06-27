#ifndef COVER_ART_GRABBER_HPP
#define COVER_ART_GRABBER_HPP

#include <vector>

#include "av/InputFormatContext.hpp"
#include "database/AudioTypes.hpp"

#include "CoverArt.hpp"


namespace CoverArt {

class Grabber
{
	public:

		static std::vector<CoverArt>	getFromInputFormatContext(const Av::InputFormatContext& input);
		static std::vector<CoverArt>	getFromTrack(Database::Track::pointer		track);
		static std::vector<CoverArt>	getFromRelease(Database::Release::pointer	release);
};

} // namespace CoverArt


#endif
