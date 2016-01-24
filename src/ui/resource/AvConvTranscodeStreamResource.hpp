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

#ifndef AVCONV_TRANSCODE_STREAM_RESOURCE_HPP
#define AVCONV_TRANSCODE_STREAM_RESOURCE_HPP

#include <iostream>
#include <memory>

#include <Wt/WResource>

#include "av/AvTranscoder.hpp"

namespace UserInterface {

class AvConvTranscodeStreamResource : public Wt::WResource
{
	public:
		AvConvTranscodeStreamResource(boost::filesystem::path p, Av::TranscodeParameters parameters, Wt::WObject *parent = 0);
		~AvConvTranscodeStreamResource();

		void handleRequest(const Wt::Http::Request& request,
				Wt::Http::Response& response);

	private:
		boost::filesystem::path		_filePath;
		Av::TranscodeParameters		_parameters;

		static const std::size_t	_bufferSize = 8192;
};

} // namespace UserInterface

#endif

