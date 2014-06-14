#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/io_new/jpeg_all.hpp>

#include "CoverArt.hpp"

namespace CoverArt {

bool
CoverArt::scale(std::size_t size)
{
	bool res = false;
	try {

		boost::gil::rgb8_image_t source;
		boost::gil::rgb8_image_t dest(size, size);

		// Read source
		{
			std::istringstream iss( std::string(_data.begin(), _data.end()));
			boost::gil::read_image(iss, source, boost::gil::jpeg_tag());
		}

		// Resize
		boost::gil::resize_view(boost::gil::const_view(source),
				boost::gil::view(dest),
				boost::gil::bilinear_sampler());

		// Output to dest
		{
			std::ostringstream oss;
			boost::gil::write_view(oss, boost::gil::const_view(dest), boost::gil::jpeg_tag());

			std::string output = oss.str();
			_data.assign(output.begin(), output.end());
		}

		res = true;
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

	return res;
}

} // namespace CoverArt
