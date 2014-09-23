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

#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/io_new/jpeg_all.hpp>

#include "logger/Logger.hpp"

#include "CoverArt.hpp"

namespace CoverArt {

bool
CoverArt::scale(std::size_t size)
{
	bool res = false;

	if (!size)
		return false;

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
		LMS_LOG(MOD_COVER, SEV_ERROR) << "Caught exception: " << e.what();
	}

	return res;
}

} // namespace CoverArt
